-- http://www.serpentine.com/blog/2007/01/31/parsing-a-simple-config-file-in-haskell/
import qualified Data.Map as M
import qualified Data.List as L
import Data.Either
import Data.Maybe
import Text.ParserCombinators.Parsec

main = putStrLn "Hello world"

data RawLine = DeletionNumber Int       -- at most, 1 allowed
             | Rule String [String]     -- an arbitrary amount allowed
             | Tape [String]            -- at most, 1 allowed
    deriving (Show)

defaultDeletionNumber = DeletionNumber 2

type RawProgram = [RawLine]

void p = do
            _ <- p
            return ()

whitespace :: Parser ()
whitespace = void $ many $ oneOf " \t"

lexeme :: Parser a -> Parser a
lexeme p = do
           x <- p
           whitespace
           return x

parseSymbol :: Parser String
parseSymbol = do
                 c  <- letter <|> char '_'
                 cs <- lexeme $ many (letter <|> digit <|> char '_')
                 return (c:cs)
                 <?> "symbol"

parseComment :: Parser ()
parseComment = do
                  char '#'
                  skipMany (noneOf "\r\n")
                  <?> "comment"

parseEol :: Parser ()
parseEol = do
              oneOf "\r\n"
              return ()
              <?> "end of line"

terminators = (parseComment <|> parseEol <|> eof)

parseDeletionNumber = option defaultDeletionNumber parseIt
    where
        parseIt = do
                     number <- lexeme $ many1 digit
                     void $ many $ oneOf " \t\r\n"
                     return . DeletionNumber $ (read number :: Int)
                     <?> "deletion number"

parseProductions :: Parser [String]
parseProductions = manyTill parseSymbol terminators

parseRule :: Parser RawLine
parseRule = do
               symbol <- parseSymbol
               void $ lexeme $ string "->"
               production <- parseProductions
               return (Rule symbol production)
               <?> "rule"

parseTape :: Parser RawLine
parseTape = do
               tape <- parseProductions
               return (Tape tape)
               <?> "tape"

parseProgram = do
                  delNumber <- parseDeletionNumber
                  void $ manyTill (void . many . oneOf $ " \t\r\n") terminators
                  rules <- many parseRule
                  void $ manyTill (void . many . oneOf $ " \t\r\n") terminators
                  tape <- parseTape
                  void $ manyTill (void . many . oneOf $ " \t\r\n") terminators
                  return (delNumber, rules, tape)

{-
 - Alright, so we need to do a few things.
 - 1) Parse in a tag system that looks something like:
 -      2 # an optional deletion number
 -      a -> b      # rules
 -      b -> c c    # rules with multiple symbols are separated by some whitespace
 -      c -> a
 -                  # blanks don't matter
 -      a b c       # an initial queue
 -      # and comments like this
 -
 - 2) Once we have this representation we'll need to renumber all the symbols
 -    from 0..n so that we know the byte-size length of these symbols.
 -
 - 3) At this point, we can serialize it.
 -}


-- while we're parsing (in the real thing) we can keep a separate map with the
-- key for EACH symbol and the value an incrementing counter (so first
-- encountered symbol gets a 0, next one a 1, etc). Then once we have the rules
-- in a map, we can translate each symbol to the value in the counter map. This
-- way each symbol gets a short representation for serialization.
program = foldr makeMap M.empty . map drop_arrow . map words $ lines "x_0 -> x_1 x_1\ns0_0 -> s1_1 s1_1\ns1_0 -> s1_1 s1_1\ny_0 -> y_1 y_1\nx_1 -> x_a1 x_a1 xp_a1\ns0_1 -> s0_a1 s0_a1\ns1_1 -> s1_a1 s1_a1\ny_1 -> y_a1 y_a1\nx_a1 -> x_b1 x_b1 x_b1\nxp_a1 -> x_b1 x_b1\ns0_a1 -> t_1\ns1_a1 -> x_b1 x_b1 t_1\ny_a1 -> y_b1\nx_b1 -> x_c1 x_c1\nt_1 -> t_a1 tp_a1\ny_b1 -> y_c1 y_c1\nx_c1 -> x_2 x_2\nt_a1 -> s1_2 s1_2\ntp_a1 -> x_2 s0_2 s0_2\ny_c1 -> y_2 y_2\nx_2 -> x_3 x_3\ns0_2 -> s1_3 s1_3\ns1_2 -> s1_3 s1_3\ny_2 -> y_3 y_3\nx_3 -> t_3 t_3\ns0_3 -> sp0_3\ns1_3 -> sp1_3 sp1_3\ny_3 -> u_3 up_3\nt_3 -> xp_3 xpp_3\nsp0_3 -> xp_3 spp0_3 spp0_3\nsp1_3 -> spp1_3 spp1_3\nu_3 -> yp_3 ypp_3\nup_3 -> ypp_3 yp_3\nxp_3 -> x_1 x_1\nxpp_3 -> x_4 x_4\nspp0_3 -> s0_4 s0_4\nspp1_3 -> s1_1 s1_1\nyp_3 -> y_1 y_1\nypp_3 -> y_4 y_4"

drop_arrow ls = head ls : drop 2 ls
makeMap (symbol:production) m = M.insert symbol production m

-- let contents = readFile "../../compiler/wmc/deref/deref_test.tag"
-- parse parseProgram "deref_test.tag" contents

--makeTranslation program = translation
