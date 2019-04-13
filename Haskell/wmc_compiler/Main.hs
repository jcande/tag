module Main where
-- https://github.com/sdiehl/write-you-a-haskell/blob/master/chapter3/calc/Parser.hs
-- https://wiki.haskell.org/Parsing_a_simple_imperative_language
import Text.Parsec
import Text.Parsec.String (Parser)
import qualified Text.Parsec.Token as Tok

import qualified Data.Map as M
import qualified Data.Set as S

import Data.Maybe (fromMaybe)
import System.Environment (getArgs)

{-
 - // the language looks like
 - >
 - <
 - label1: +
 - -
 - jmp label1 // comment to the end of line
 - > > > >
 - label2:
 - /* C-style comment. */
 - jmp label2, label1
 -}


-- XXX we'll need to keep a mapping from labels to offset
data Stmt = Shift ShiftOp
          | Write WOp
          | Label String
          | Jmp BranchTarget BranchTarget
  deriving (Show)

type Insn = (Integer, Stmt)
type LabelMap = M.Map String Integer

data WOp = Set
         | Unset
  deriving (Show)

data ShiftOp = SLeft
             | SRight
  deriving (Show)

-- NextAddress just means that the jmp was a nop so we fallthrough to the next
-- instruction
data BranchTarget = NextAddress
                  | Name String
  deriving (Show)

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
  , Tok.reservedNames   = ["jmp"]
  , Tok.reservedOpNames = [",", ":", ">", "<", "+", "-"]
  , Tok.caseSensitive   = True
  }

lexer :: Tok.TokenParser ()
lexer = Tok.makeTokenParser langDef

-- useful extractors
identifier = Tok.identifier lexer
allWhiteSpace = Tok.whiteSpace lexer

reserved = Tok.reserved lexer
reservedOp = Tok.reservedOp lexer

integer = Tok.integer lexer
semi = Tok.semi lexer

whileParser :: Parser [Stmt]
whileParser = allWhiteSpace >> statement

statement :: Parser [Stmt]
statement = sepEndBy statement' spaces
          <?> "statement"
    where statement' = shiftStmt
                     <|> wStmt
                     <|> labelStmt
                     <|> jmpStmt

shiftStmt :: Parser Stmt
shiftStmt = leftShift
          <|> rightShift
          <?> "shift statement"
    where
        leftShift = reservedOp "<" >> return (Shift SLeft)
        rightShift = reservedOp ">" >> return (Shift SRight)

wStmt :: Parser Stmt
wStmt = set
      <|> unset
      <?> "write statement"
    where
        set = reservedOp "+" >> return (Write Set)
        unset = reservedOp "-" >> return (Write Unset)

labelStmt :: Parser Stmt
labelStmt =
    do name <- identifier
       reservedOp ":"
       return $ Label name
       <?> "label statement"

jmpStmt :: Parser Stmt
jmpStmt =
    do reserved "jmp"
       conditionalTarget <- identifier
       defaultTarget <- fallthroughTarget
       return $ Jmp (Name conditionalTarget) (defaultTarget)
       <?> "jmp statement"
    where fallthroughTarget = optionalTarget <|> return NextAddress
          optionalTarget =
            do reservedOp ","
               target <- identifier
               return (Name target)

parseString :: String -> [Stmt]
parseString str =
    case parse whileParser "" str of
        Left e  -> error (show e)
        Right r -> r

parseFile :: FilePath -> IO [Stmt]
parseFile filename =
  do program <- readFile filename
     case parse whileParser filename program of
       Left e  -> print e >> fail "parse error"
       Right r -> return r

program = [Write Set,Label "top",Shift SRight,Write Set,Jmp (Name "top") NextAddress]

enrich :: [Stmt] -> (LabelMap, [Insn])
enrich program =
    let (labels, _, insns) = foldl' go (M.empty, 0, []) program
    in (labels, insns)
        where go (labels, addr, enriched) stmt =
                let enrich = (labels, addr+1, (addr, stmt) : enriched)
                    addLabel label = (M.insert label addr labels, addr, enriched)
                in case stmt of
                        Label label -> addLabel label
                        otherwise   -> enrich

-- XXX should make this return Either and the failing label name
enrichmentIsValid labels = all validInstruction
    where
        validInstruction insn =
            let (addr, insn') = insn
            in case insn' of
                    Shift _                         -> True
                    Write _                         -> True
                    Jmp (Name label1) (Name label2) -> (M.lookup label1 labels /= Nothing) &&
                                                       (M.lookup label2 labels /= Nothing)
                    Jmp (Name label1) NextAddress   -> M.lookup label1 labels /= Nothing
                    otherwise                       -> False

-- 3 "x_" => "x_3"
makeSymbol :: Integer -> String -> String
makeSymbol addr name = name ++ (show addr)

-- "x_0" "x_1 x_1" => "x_0 -> x_1 x_1"
makeRule :: String -> String -> String
makeRule symbol appendant = symbol ++ " -> " ++ appendant

makeWriteSet :: Integer -> [String]
makeWriteSet addr =
    let symbols = ["x_", "s0_", "s1_", "y_"]
        [  x, s0,   s1,   y] = map (makeSymbol addr) symbols
        [x_n,  _, s1_n, y_n] = map (makeSymbol (addr+1)) symbols
    in [ makeRule  x (unwords [ x_n,  x_n])
       , makeRule s0 (unwords [s1_n, s1_n])
       , makeRule s1 (unwords [s1_n, s1_n])
       , makeRule  y (unwords [ y_n,  y_n])
       ]

makeWriteUnset :: Integer -> [String]
makeWriteUnset addr =
    let symbols = ["x_", "s0_", "s1_", "y_"]
        [  x,   s0, s1,   y] = map (makeSymbol addr) symbols
        [x_n, s0_n,  _, y_n] = map (makeSymbol (addr+1)) symbols
    in [ makeRule  x (unwords [ x_n,  x_n])
       , makeRule s0 (unwords [s0_n, s0_n])
       , makeRule s1 (unwords [s0_n, s0_n])
       , makeRule  y (unwords [ y_n,  y_n])
       ]

makeShiftLeft addr =
    let xs  = [ "x_", "x_a", "x_b", "x_c", "x_d" ]
        s0s = [ "s0_", "s0_a", "s0_b" ]
        ts  = [ "t_", "t_1", "t_2" ]
        s1s = [ "s1_", "s1_a", "s1_b" ]
        ys  = [ "y_", "y_a", "y'_a", "y_b", "y_c", "y_d" ]

        [ x, x_a, x_b, x_c, x_d ]       = map (makeSymbol addr) xs
        [ s0, s0_a, s0_b ]              = map (makeSymbol addr) s0s
        [ t, t_1, t_2 ]                 = map (makeSymbol addr) ts
        [ s1, s1_a, s1_b ]              = map (makeSymbol addr) s1s
        [ y, y_a, y'_a, y_b, y_c, y_d ] = map (makeSymbol addr) ys

        [ x_n, _, _, _, _ ]    = map (makeSymbol (addr+1)) xs
        [ s0_n, _, _ ]         = map (makeSymbol (addr+1)) s0s
        [ s1_n, _, _ ]         = map (makeSymbol (addr+1)) s1s
        [ y_n, _, _, _, _, _ ] = map (makeSymbol (addr+1)) ys

        -- (1)
    in [ makeRule x     (unwords [x_a,  x_a])
       , makeRule s0    (unwords [s0_a, s0_a])
       , makeRule s1    (unwords [s1_a, s1_a])
       , makeRule y     (unwords [y_a,  y_a, y'_a])
       , makeRule x_a   (unwords [x_b,  x_b])
       , makeRule s0_a  (unwords [s0_b, s0_b])
       , makeRule s1_a  (unwords [s1_b, s1_b])

        -- (2)
       , makeRule y_a   (unwords [y_b,  y_b, y_b])
       , makeRule y'_a  (unwords [y_b,  y_b])
       , makeRule x_b   (unwords [x_c,  x_c])

        -- (3)
        , makeRule s0_b (unwords [t])
        , makeRule s1_b (unwords [t,    y_c, y_c])
        , makeRule y_b  (unwords [y_c,  y_c])
        , makeRule x_c  (unwords [x_d])

        -- (4)
       , makeRule t     (unwords [t_1,  t_2])
       , makeRule y_c   (unwords [y_d,  y_d])

        -- (5)
       , makeRule x_d   (unwords [x_n,  x_n])

        -- (6)
       , makeRule t_2   (unwords [x_n,  s0_n, s0_n])
       , makeRule t_1   (unwords [s1_n, s1_n])
       , makeRule y_d   (unwords [y_n,  y_n])
       ]

makeShiftRight addr =
    let xs  = [ "x_", "x_a", "x'_a", "x_b", "x_c" ]
        s0s = [ "s0_", "s0_a" ]
        ts  = [ "t_", "t_a", "t'_a" ]
        s1s = [ "s1_", "s1_a" ]
        ys  = [ "y_", "y_a", "y_b", "y_c" ]

        [ x, x_a, x'_a, x_b, x_c ] = map (makeSymbol addr) xs
        [ s0, s0_a ]               = map (makeSymbol addr) s0s
        [ t, t_a, t'_a ]           = map (makeSymbol addr) ts
        [ s1, s1_a ]               = map (makeSymbol addr) s1s
        [ y, y_a, y_b, y_c ]       = map (makeSymbol addr) ys

        [ x_n, _, _, _, _ ] = map (makeSymbol (addr+1)) xs
        [ s0_n, _ ]         = map (makeSymbol (addr+1)) s0s
        [ s1_n, _ ]         = map (makeSymbol (addr+1)) s1s
        [ y_n, _, _, _ ]    = map (makeSymbol (addr+1)) ys

        -- (1)
    in [ makeRule x    (unwords [x_a,  x_a, x'_a])
       , makeRule s0   (unwords [s0_a, s0_a])
       , makeRule s1   (unwords [s1_a, s1_a])
       , makeRule y    (unwords [y_a,  y_a])

        -- (2)
       , makeRule x_a  (unwords [x_b,  x_b, x_b])
       , makeRule x'_a (unwords [x_b,  x_b])

        -- (3)
       , makeRule s0_a (unwords [t])
       , makeRule s1_a (unwords [x_b,  x_b, t])
       , makeRule y_a  (unwords [y_b])

        -- (4)
       , makeRule x_b  (unwords [x_c,  x_c])

        -- (5)
       , makeRule t    (unwords [t_a,  t'_a])
       , makeRule y_b  (unwords [y_c,  y_c])

        -- (6)
       , makeRule x_c  (unwords [x_n,  x_n])

        -- (7)
       , makeRule t_a  (unwords [s1_n, s1_n])
       , makeRule t'_a (unwords [x_n, s0_n, s0_n])
       , makeRule y_c  (unwords [y_n, y_n])
       ]

makeJump addr target fallthrough =
    let xs  = [ "x_", "x'_", "x''_" ]
        ts  = [ "t_" ]
        s0s = [ "s0_", "s0'_", "s0''_" ]
        s1s = [ "s1_", "s1'_", "s1''_" ]
        ys  = [ "y_", "y'_", "y''_" ]
        us  = [ "u_", "u'_" ]

        [ x, x', x'' ]    = map (makeSymbol addr) xs
        [ t ]             = map (makeSymbol addr) ts
        [ s0, s0', s0'' ] = map (makeSymbol addr) s0s
        [ s1, s1', s1'' ] = map (makeSymbol addr) s1s
        [ y, y', y'' ]    = map (makeSymbol addr) ys
        [ u, u' ]         = map (makeSymbol addr) us

        [ x_t, _, _ ]  = map (makeSymbol target) xs
        [ s0_t, _, _ ] = map (makeSymbol target) s0s
        [ s1_t, _, _ ] = map (makeSymbol target) s1s
        [ y_t, _, _ ]  = map (makeSymbol target) ys

        [ x_f, _, _ ]  = map (makeSymbol fallthrough) xs
        [ s0_f, _, _ ] = map (makeSymbol fallthrough) s0s
        [ s1_f, _, _ ] = map (makeSymbol fallthrough) s1s
        [ y_f, _, _ ]  = map (makeSymbol fallthrough) ys

    in [ makeRule x    (unwords [t,    t])
       , makeRule s0   (unwords [s0'])
       , makeRule s1   (unwords [s1',  s1'])
       , makeRule y    (unwords [u,    u'])

       , makeRule t    (unwords [x',   x''])

       , makeRule s0'  (unwords [x',   s0'', s0''])
       , makeRule s1'  (unwords [s1'', s1''])
       , makeRule u    (unwords [y',   y''])
       , makeRule u'   (unwords [y'',  y''])

       , makeRule x'   (unwords [x_t,  x_t])
       , makeRule x''  (unwords [x_f,  x_f])
       , makeRule s0'' (unwords [s0_f, s0_f])
       , makeRule s1'' (unwords [s1_t, s1_t])
       , makeRule y'   (unwords [y_t,  y_t])
       , makeRule y''  (unwords [y_f,  y_f])
       ]

compile :: LabelMap -> [Insn] -> [[String]]
compile labels instructions = map translate instructions
    where
        invalidAddress = fromIntegral $ length instructions
        translate (addr, insn) =
            case insn of
                 Write Set      -> makeWriteSet addr
                 Write Unset    -> makeWriteUnset addr
                 Shift SLeft    -> makeShiftLeft addr
                 Shift SRight   -> makeShiftRight addr
                 Jmp true false -> let addr1 = getAddr addr true
                                       addr2 = getAddr addr false
                                   in makeJump addr addr1 addr2
                 Label _        -> []
        getAddr :: Integer -> BranchTarget -> Integer
        getAddr fromAddr (Name label) = fromMaybe invalidAddress (M.lookup label labels)
        getAddr fromAddr NextAddress  = fromAddr + 1


{-

-- to compile
program <- parseFile "test2.wm"
let (l, i) = enrich program
let obj = concat . map (++";\n") $ concat (compile l i)
writeFile "test2.tag" ("2;\n" ++ obj ++ ": s0_0 s0_0;\n")

-}
