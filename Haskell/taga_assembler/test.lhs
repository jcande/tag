1 Imports
First let's specify the name of the module:

> module ParseWhile where
And then import the necessary libraries:

> import System.IO
> import Control.Monad
> import Text.ParserCombinators.Parsec
> import Text.ParserCombinators.Parsec.Expr
> import Text.ParserCombinators.Parsec.Language
> import qualified Text.ParserCombinators.Parsec.Token as Token
2 The language
The grammar for expressions is defined as follows:


a  ::= x | n | - a | a opa a

b  ::= true | false | not b | b opb b | a opr a

opa ::= + | - | * | /

opb ::= and | or

opr ::= > | <

Note that we have three groups of operators - arithmetic, booloan and relational ones.

And now the definition of statements:


S  ::= x := a | skip | S1; S2 | ( S ) | if b then S1 else S2 | while b do S

We probably want to parse that into some internal representation of the language (abstract syntax tree). Therefore we need to define the data structures for the expressions and statements.

3 Data structures
We need to take care of boolean and arithmetic expressions and the appropriate operators. First let's look at the boolean expressions:

> data BExpr = BoolConst Bool
>            | Not BExpr
>            | BBinary BBinOp BExpr BExpr
>            | RBinary RBinOp AExpr AExpr
>             deriving (Show)
Binary booloan operators:

> data BBinOp = And | Or deriving (Show)
Relational operators:

> data RBinOp = Greater | Less deriving (Show)
Now we define the types for arithmetic expressions:

> data AExpr = Var String
>            | IntConst Integer
>            | Neg AExpr
>            | ABinary ABinOp AExpr AExpr
>              deriving (Show)
And arithmetic operators:

> data ABinOp = Add
>             | Subtract
>             | Multiply
>             | Divide
>               deriving (Show)
Finally let's take care of the statements:

> data Stmt = Seq [Stmt]
>           | Assign String AExpr
>           | If BExpr Stmt Stmt
>           | While BExpr Stmt
>           | Skip
>             deriving (Show)
4 Lexer
Having all the data structures we can go on with writing the code to do actual parsing. First of all we create the language definition using Haskell's record syntax and the constructor emptyDef from Text.ParserCombinators.Parsec.Language:
> languageDef =
>   emptyDef { Token.commentStart    = "/*"
>            , Token.commentEnd      = "*/"
>            , Token.commentLine     = "//"
>            , Token.identStart      = letter
>            , Token.identLetter     = alphaNum
>            , Token.reservedNames   = [ "if"
>                                      , "then"
>                                      , "else"
>                                      , "while"
>                                      , "do"
>                                      , "skip"
>                                      , "true"
>                                      , "false"
>                                      , "not"
>                                      , "and"
>                                      , "or"
>                                      ]
>            , Token.reservedOpNames = ["+", "-", "*", "/", ":="
>                                      , "<", ">", "and", "or", "not"
>                                      ]
>            }
This creates a language definition that accepts the C-style comments, requires that the identifiers start with a letter, and end with alphanumeric characters. Moreover there is a number of reserved names, that cannot be used by the identifiers.

Having the above definition we can create a lexer:

> lexer = Token.makeTokenParser languageDef
lexer contains a number of lexical parsers, that we can use to parse identifiers, reserved words/operations, etc. Now we can select/extract them in the following way:

> identifier = Token.identifier lexer -- parses an identifier
> reserved   = Token.reserved   lexer -- parses a reserved name
> reservedOp = Token.reservedOp lexer -- parses an operator
> parens     = Token.parens     lexer -- parses surrounding parenthesis:
>                                     --   parens p
>                                     -- takes care of the parenthesis and
>                                     -- uses p to parse what's inside them
> integer    = Token.integer    lexer -- parses an integer
> semi       = Token.semi       lexer -- parses a semicolon
> whiteSpace = Token.whiteSpace lexer -- parses whitespace
This isn't really necessary, but should make the code much more readable (also this is the reason why we used the qualified import of Text.ParserCombinators.Parsec.Token). Now we can use them to parse the source code at the token level. One of the nice features of these parsers is that they take care of all whitespace after the tokens.
5 Main parser
As already mentioned a program in this language is simply a statement, so the main parser should basically only parse a statement. But remember to take care of initial whitespace - our parsers only get rid of whitespace after the tokens!

> whileParser :: Parser Stmt
> whileParser = whiteSpace >> statement
Now because any statement might be actually a sequence of statements separated

by semicolon, we use sepBy1 to parse at least one statement. The
result is a list of statements. We also allow grouping statements by the parenthesis, which is useful, for instance, in the while loop.

> statement :: Parser Stmt
> statement =   parens statement
>           <|> sequenceOfStmt
 
> sequenceOfStmt =
>   do list <- (sepBy1 statement' semi)
>      -- If there's only one statement return it without using Seq.
>      return $ if length list == 1 then head list else Seq list
Now a single statement is quite simple, it's either an if conditional, a while

loop, an assignment or simply a skip statement. We use <|> to express choice. So a <|> b will first try parser a
and if it fails (but without actually consuming any input) then parser

b will be used. Note: this means that the order is important.
> statement' :: Parser Stmt
> statement' =   ifStmt
>            <|> whileStmt
>            <|> skipStmt
>            <|> assignStmt
If you have a parser that might fail after consuming some input, and you still

want to try the next parser, you should look into try combinator. For instance try p <|> q will try parsing with p and if it fails, even after consuming the input, the q parser will be used as if nothing has been consumed by p.
Now let's define the parsers for all the possible statements. This is quite straightforward as we just use the parsers from the lexer and then use all the necessary information to create appropriate data structures.

> ifStmt :: Parser Stmt
> ifStmt =
>   do reserved "if"
>      cond  <- bExpression
>      reserved "then"
>      stmt1 <- statement
>      reserved "else"
>      stmt2 <- statement
>      return $ If cond stmt1 stmt2
 
> whileStmt :: Parser Stmt
> whileStmt =
>   do reserved "while"
>      cond <- bExpression
>      reserved "do"
>      stmt <- statement
>      return $ While cond stmt
 
> assignStmt :: Parser Stmt
> assignStmt =
>   do var  <- identifier
>      reservedOp ":="
>      expr <- aExpression
>      return $ Assign var expr
 
> skipStmt :: Parser Stmt
> skipStmt = reserved "skip" >> return Skip
6 Expressions
What's left is to parse the expressions. Fortunately Parsec provides a very easy way to do that. Let's define the arithmetic and boolean expressions:

> aExpression :: Parser AExpr
> aExpression = buildExpressionParser aOperators aTerm
 
> bExpression :: Parser BExpr
> bExpression = buildExpressionParser bOperators bTerm
Now we have to define the lists with operator precedence, associativity and what constructors to use in each case.

> aOperators = [ [Prefix (reservedOp "-"   >> return (Neg             ))          ]
>              , [Infix  (reservedOp "*"   >> return (ABinary Multiply)) AssocLeft,
>                 Infix  (reservedOp "/"   >> return (ABinary Divide  )) AssocLeft]
>              , [Infix  (reservedOp "+"   >> return (ABinary Add     )) AssocLeft,
>                 Infix  (reservedOp "-"   >> return (ABinary Subtract)) AssocLeft]
>               ]
 
> bOperators = [ [Prefix (reservedOp "not" >> return (Not             ))          ]
>              , [Infix  (reservedOp "and" >> return (BBinary And     )) AssocLeft,
>                 Infix  (reservedOp "or"  >> return (BBinary Or      )) AssocLeft]
>              ]
In case of Prefix operators it is enough to specify which one should be parsed and what is the associated data constructor. Infix operators are defined similarly, but it's necessary to add information about associativity. Note that the operator precedence depends only on the order of the elements in the list.

Finally we have to define the terms. In case of arithmetic expressions, it is quite simple:

> aTerm =  parens aExpression
>      <|> liftM Var identifier
>      <|> liftM IntConst integer
However, the term in a boolean expression is a bit more tricky. In this case, a term can also be an expression with relational operator consisting of arithmetic expressions.

> bTerm =  parens bExpression
>      <|> (reserved "true"  >> return (BoolConst True ))
>      <|> (reserved "false" >> return (BoolConst False))
>      <|> rExpression
Therefore we have to define a parser for relational expressions:

> rExpression =
>   do a1 <- aExpression
>      op <- relation
>      a2 <- aExpression
>      return $ RBinary op a1 a2
 
> relation =   (reservedOp ">" >> return Greater)
>          <|> (reservedOp "<" >> return Less)
And that's it. We have a quite simple parser able to parse a few statements and arithmetic/boolean expressions.

7 Notes
If you want to experiment with the parser inside ghci, these functions might be handy:

> parseString :: String -> Stmt
> parseString str =
>   case parse whileParser "" str of
>     Left e  -> error $ show e
>     Right r -> r
 
> parseFile :: String -> IO Stmt
> parseFile file =
>   do program  <- readFile file
>      case parse whileParser "" program of
>        Left e  -> print e >> fail "parse error"
>        Right r -> return r
Now you can simply load the module in ghci and then do

ast <- parseFile "<filename>" to parse a file and get the
result if parsing was successful. If you already have a string with

the program, you can use parseString.
