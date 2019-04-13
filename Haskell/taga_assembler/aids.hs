-- https://github.com/sdiehl/write-you-a-haskell/blob/master/chapter3/calc/Parser.hs
-- https://wiki.haskell.org/Parsing_a_simple_imperative_language
import Text.Parsec
import Text.Parsec.String (Parser)
import qualified Text.Parsec.Token as Tok

import qualified Data.Map as M
import qualified Data.Set as S

import Data.Maybe (fromMaybe)
import System.Environment (getArgs)

import qualified Data.ByteString as B
import qualified Data.ByteString.Lazy as LB
import Data.Binary
import Data.Binary.Put
import Data.Binary.Get
import Data.Bits


{-
 - This parses "tag" files in the format of:
 -
 - [integer;]  # optional deletion number
 - {a -> b c;} # arbitrary number of rules
 - [: a b c;]  # optional initial tape
 -
 -}

type RawSymbol = String
type RawRule = (RawSymbol, [RawSymbol])

data RawProgram = RawProgram
             { deletionNumber :: Integer
             , rules ::[RawRule]
             , tape :: [RawSymbol]
             , uniqueSymbols :: Integer
             }
  deriving (Eq, Show)

{-
data Rules = Rules
           {
data Program = Program
             { ruleCount        :: Word64 -- total number of rules
             , symbolSize       :: Word32 -- the size (in bytes) of a single symbol
             , queueSize        :: Word32 -- the size (in bytes) of the input
             --, deletionNumber   :: Word32
             , rules            :: [Rules] -- the actual rules
             , queue            :: B.ByteString -- the tape
             }
-}

langDef :: Tok.LanguageDef ()
langDef = Tok.LanguageDef
  { Tok.commentStart    = "/*"
  , Tok.commentEnd      = "*/"
  , Tok.commentLine     = "#"
  , Tok.nestedComments  = True
  , Tok.identStart      = letter
  , Tok.identLetter     = alphaNum <|> oneOf "_'"
  , Tok.opStart         = oneOf ":!#$%&*+./<=>?@\\^|-~"
  , Tok.opLetter        = oneOf ":!#$%&*+./<=>?@\\^|-~"
  , Tok.reservedNames   = []
  , Tok.reservedOpNames = ["->", ":"]
  , Tok.caseSensitive   = True
  }

lexer :: Tok.TokenParser ()
lexer = Tok.makeTokenParser langDef

-- useful extractors
identifier = Tok.identifier lexer
allWhiteSpace = Tok.whiteSpace lexer
integer    = Tok.integer lexer
semi = Tok.semi lexer

reserved :: String -> Parser ()
reserved   = Tok.reserved lexer
reservedOp :: String -> Parser ()
reservedOp = Tok.reservedOp lexer

defaultDeletionNumber = 2

parseDeletionNumber = option defaultDeletionNumber go
  where
    go =
      do n <- integer
         -- I hate the semicolon but I cannot figure out how to make it parse
         -- correctly without it.
         Tok.semi lexer
         return n
         <?> "deletion number"

parseProductions =
  do productions <- (sepBy identifier allWhiteSpace)
     -- I hate the semicolon but I cannot figure out how to make it parse
     -- correctly without it.
     Tok.semi lexer
     return productions
     <?> "productions"

parseRule =
  do symbol <- identifier
     reservedOp "->"
     productions <- parseProductions
     return (symbol, productions)
     <?> "rule"

parseTape =
     -- I hate the colon but I cannot figure out how to make it parse
     -- correctly without it.
  do reservedOp ":"
     tape <- parseProductions
     return tape
     <?> "tape"

--parseProgram :: Parser a
parseProgram =
  do allWhiteSpace
     deletionNo <- parseDeletionNumber
     rules <- many parseRule
     tape <- option [] parseTape
     eof
     return (deletionNo, rules, tape)
     <?> "program"

-- XXX after we parse the file we should verify some stuff about it!
parseFile filename =
  do program <- readFile filename
     case parse parseProgram filename program of
       Left e  -> print e >> fail "parse error"
       Right r -> return r

-- XXX we should keep the translation mapping for debugging purposes
renameSymbols :: M.Map RawSymbol [RawSymbol] -> [RawSymbol] -> (M.Map Integer [Integer], [Integer], Integer, M.Map String Integer)
renameSymbols rules tape =
  let state = fromMaybe empty $ M.foldWithKey renameRule (Just empty) rules
      (rules', (translator, max)) = state
      tape' = fromMaybe [] . renameProduction translator $ tape
  in (rules', tape', max, translator)
    where
        -- (renamed, (symbol->int, count))
        empty :: (M.Map k1 a2, (M.Map String Integer, Integer))
        empty = (M.empty, (M.empty, 0))

        renameRule :: String -> [String] -> Maybe (M.Map Integer [Integer], (M.Map String Integer, Integer)) -> Maybe (M.Map Integer [Integer], (M.Map String Integer, Integer))
        renameRule _ _ Nothing = Nothing
        renameRule key value (Just (renamed, state)) =
            let state' = foldr symbolMap state (key:value)
                translator' = fst state'
                key' = M.lookup key translator'
                value' = renameProduction translator' value
            -- want to lift fst?
            in key' >>= \k -> value' >>= \v -> Just (M.insert k v renamed, state')

        renameProduction translator = sequence . map (symbolLookup translator)
        symbolLookup = flip M.lookup

        symbolMap :: (Num a, Ord k) => k -> (M.Map k a, a) -> (M.Map k a, a)
        symbolMap sym (translated, i) =
          let translated' = M.insert sym i translated
          in case (M.lookup sym translated) of
                   Just _  -> (translated, i)    -- this key already exists, we're done
                   Nothing -> (translated', i+1) -- add the key

simpleValidation :: (Integer, [RawRule], [RawSymbol]) -> Either String (Integer, [RawRule], [RawSymbol])
simpleValidation all@(deletion, rules, _) =
    let symbols = map fst rules
        unique  = S.fromList symbols
        errors  = [ (length symbols /= length unique, "You have duplicate symbols (left of ->) in your rules.")
                  , (deletion < 2, "Your deletion number must be >= 2.")
                  ]
    in case (filter fst errors) of
            (_, reason):_ -> Left reason
            otherwise     -> Right all

{-
main =
  do args <- getArgs
     case args of
          [filename]         -> assemble filename "bin" >> return ()
          [filename, output] -> assemble filename output >> return ()
          otherwise          -> putStrLn "./taga program.tag [output.bin]"
    where
        assemble input output =
            do raw <- parseFile input
               chain <- raw >>= pure . simpleValidation
               case chain of
                    Right whatever -> return . undefined -- write to output
                    Left error     -> return $ putStrLn error
-}

p = RawProgram { deletionNumber = 2
               , rules = [("s0_0",["s1_1","s1_1"]),("s0_1",["s0_a1","s0_a1"]),("s0_2",["s1_3","s1_3"]),("s0_3",["sp0_3"]),("s0_a1",["t_1"]),("s1_0",["s1_1","s1_1"]),("s1_1",["s1_a1","s1_a1"]),("s1_2",["s1_3","s1_3"]),("s1_3",["sp1_3","sp1_3"]),("s1_a1",["x_b1","x_b1","t_1"]),("sp0_3",["xp_3","spp0_3","spp0_3"]),("sp1_3",["spp1_3","spp1_3"]),("spp0_3",["s0_4","s0_4"]),("spp1_3",["s1_1","s1_1"]),("t_1",["t_a1","tp_a1"]),("t_3",["xp_3","xpp_3"]),("t_a1",["s1_2","s1_2"]),("tp_a1",["x_2","s0_2","s0_2"]),("u_3",["yp_3","ypp_3"]),("up_3",["ypp_3","yp_3"]),("x_0",["x_1","x_1"]),("x_1",["x_a1","x_a1","xp_a1"]),("x_2",["x_3","x_3"]),("x_3",["t_3","t_3"]),("x_a1",["x_b1","x_b1","x_b1"]),("x_b1",["x_c1","x_c1"]),("x_c1",["x_2","x_2"]),("xp_3",["x_1","x_1"]),("xp_a1",["x_b1","x_b1"]),("xpp_3",["x_4","x_4"]),("y_0",["y_1","y_1"]),("y_1",["y_a1","y_a1"]),("y_2",["y_3","y_3"]),("y_3",["u_3","up_3"]),("y_a1",["y_b1"]),("y_b1",["y_c1","y_c1"]),("y_c1",["y_2","y_2"]),("yp_3",["y_1","y_1"]),("ypp_3",["y_4","y_4"])]
               --, tape = ["s0_0","s0_0"]
               , tape = []
               , uniqueSymbols = 42
               }
r = rules p
k = map fst r

assembleToFile program filename =
    let bs = runPut (serialize program)
    in LB.writeFile filename bs

smallestByteBoundary x = head . filter (\i -> x < (2^(8*i))) $ [1..]

(p', translator) = let (r', t', max, translator) = renameSymbols (M.fromList $ rules p) (tape p)
     in ((deletionNumber p, r', t', smallestByteBoundary max), translator)

serialize :: (Integer, M.Map Integer [Integer], [Integer], Integer) -> Put
serialize (delNo, rules, tape, max) =
  let rules' = M.toList rules
  in do
       generateHeader
       sequence_ $ map generateRules rules'
       sequence_ $ map generateSymbol tape
    where
        generateHeader = do
                           putWord64le . fromIntegral $ length rules
                           putWord32le $ fromIntegral max
                           putWord32le $ fromIntegral (max * (fromIntegral $ length tape))
        generateRules (symbol, appendant) = do
                                              generateRuleHeader appendant
                                              generateSymbol symbol
                                              sequence_ $ map generateSymbol appendant
        generateRuleHeader appendant =
            let len = fromIntegral $ length appendant
            in do putWord16le . fromIntegral $ len * max
        generateSymbol symbol =
            let symbol' = asBytes symbol
            in do sequence_ $ map (putWord8 . fromIntegral) symbol'
        asBytes :: Integer -> [Integer]
        asBytes = take (fromIntegral max) . map (.&. 0xff) . iterate (\x -> shift x (-8))


{-
-- to compile
p <- parseFile "test2.tag"
let (_, r, t) = p
let (r', t', max, translator) = renameSymbols (M.fromList r) (t)
let bin = runPut $ serialize (2, r', t', 1)
LB.writeFile "output.bin" bin

-- to debug (map from raw numeric values back to symbol names)
let inverse = M.fromList . map (\(x, y) -> (y, x)) $ M.toList translator
let query = \k -> M.lookup k inverse
-}
