--module ParseWhile where
--https://wiki.haskell.org/Parsing_a_simple_imperative_language
--https://jakewheat.github.io/intro_to_parsing/

import System.IO
import Control.Monad

import Text.Parsec.String (Parser)
import Text.Parsec.String.Char (anyChar)
import Text.Parsec.String.Char
import FunctionsAndTypesForParsing (regularParse, parseWithEof, parseWithLeftOver)
import Data.Char
import Text.Parsec.String.Combinator (many1)


{-
import Text.ParserCombinators.Parsec
import Text.ParserCombinators.Parsec.Expr
import Text.ParserCombinators.Parsec.Language



del ::= digits
symbol ::= identifier
rules ::= symbol -> { symbol  }
tape ::= { symbol }

data Expr = DelN Int
          | Rule String [String]
          | Tape [String]
          deriving Show

data Rule = Rule { symbol      :: String
                 , productions :: [String]
                 }
          deriving Show

type Tape = [String]

type Program = [Expr]

parseDelN :: Parser Expr
parseDelN = do
  n <- many digit
  return (DelN n)
  <?> "deletion number"
-}

data Expr = DelN Int
          | Rule String [String]
          | Tape [String]
          deriving Show


{-
langDef :: Tok.LanguageDef ()
langDef = Tok.LanguageDef
        { Tok.commentStart    = "/*"
        , Tok.commentEnd      = "*/"
        , Tok.commentLine     = "#"
        , Tok.nestedComments  = False
        , Tok.identStart      = letter
        , Tok.identLetter     = alphaNum <|> oneOf "_'"
        , Tok.opStart         = oneOf ":!#$%&*+./<=>?@\\^|-~"
        , Tok.opLetter        = oneOf ":!#$%&*+./<=>?@\\^|-~"
        , Tok.reservedNames   = []
        , Tok.reservedOpNames = ["->"]
        , Tok.caseSensitive   = True
        }


lexer :: Tok.TokenParser ()
lexer = Tok.makeTokenParser langDef

parens :: Parser a -> Parser a
parens = Tok.parens lexer

reserved :: String -> Parser ()
reserved = Tok.reserved lexer

semiSep :: Parser a -> Parser [a]
semiSep = Tok.semiSep lexer

reservedOp :: String -> Parser ()
reservedOp = Tok.reservedOp lexer

prefixOp :: String -> (a -> a) -> Ex.Operator String () Identity a
prefixOp s f = Ex.Prefix (reservedOp s >> return f)

table :: Ex.OperatorTable String () Identity Expr
table = [
        ]


-}
