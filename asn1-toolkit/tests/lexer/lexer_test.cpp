#include <asn1/frontend/lexer.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

struct LexResult {
  asn1::SourceFile file;
  std::vector<asn1::Token> tokens;
};

LexResult lex_all(const std::string& text, asn1::Diagnostics& diag,
                  const std::string& path = "test.asn") {
  LexResult result;
  result.file = asn1::SourceFile::from_string(path, text);
  asn1::Lexer lexer(result.file, diag);
  for (;;) {
    asn1::Token tok = lexer.next();
    result.tokens.push_back(tok);
    if (tok.kind == asn1::TokenKind::EndOfFile) {
      break;
    }
  }
  return result;
}

std::vector<asn1::TokenKind> kinds_of(const std::vector<asn1::Token>& tokens) {
  std::vector<asn1::TokenKind> kinds;
  kinds.reserve(tokens.size());
  for (const auto& t : tokens) {
    kinds.push_back(t.kind);
  }
  return kinds;
}

}  // namespace

TEST(SourceFile, LocationAtTracksLinesAndColumns) {
  auto file = asn1::SourceFile::from_string("a.asn", "ab\ncde\nf");
  auto loc0 = file.location_at(0);
  EXPECT_EQ(loc0.line, 1);
  EXPECT_EQ(loc0.column, 1);

  auto loc_nl = file.location_at(2);  // '\n'
  EXPECT_EQ(loc_nl.line, 1);
  EXPECT_EQ(loc_nl.column, 3);

  auto loc_c = file.location_at(3);  // 'c'
  EXPECT_EQ(loc_c.line, 2);
  EXPECT_EQ(loc_c.column, 1);

  auto loc_f = file.location_at(7);  // 'f'
  EXPECT_EQ(loc_f.line, 3);
  EXPECT_EQ(loc_f.column, 1);
}

TEST(Diagnostics, FormatsFileLineColumn) {
  asn1::Diagnostics diag;
  asn1::SourceRange where;
  where.begin = {"example.asn", 47, 12};
  where.end = where.begin;
  diag.error(where, "undefined ASN.1 type 'FooBar'");

  ASSERT_FALSE(diag.ok());
  EXPECT_EQ(diag.format(diag.items().front()),
            "example.asn:47:12:\nerror: undefined ASN.1 type 'FooBar'");
}

TEST(Lexer, EmptyInputYieldsEof) {
  asn1::Diagnostics diag;
  auto result = lex_all("", diag);
  ASSERT_EQ(result.tokens.size(), 1u);
  EXPECT_EQ(result.tokens[0].kind, asn1::TokenKind::EndOfFile);
  EXPECT_TRUE(diag.ok());
}

TEST(Lexer, TypeReferenceVsIdentifier) {
  asn1::Diagnostics diag;
  auto result = lex_all("Person id age", diag);
  auto kinds = kinds_of(result.tokens);
  EXPECT_EQ(kinds, (std::vector<asn1::TokenKind>{
                       asn1::TokenKind::TypeReference,
                       asn1::TokenKind::Identifier,
                       asn1::TokenKind::Identifier,
                       asn1::TokenKind::EndOfFile,
                   }));
  EXPECT_EQ(result.tokens[0].text, "Person");
  EXPECT_EQ(result.tokens[1].text, "id");
}

TEST(Lexer, KeywordsAreNotTypeReferences) {
  asn1::Diagnostics diag;
  auto result = lex_all("INTEGER BOOLEAN SEQUENCE CHOICE CLASS", diag);
  auto kinds = kinds_of(result.tokens);
  EXPECT_EQ(kinds, (std::vector<asn1::TokenKind>{
                       asn1::TokenKind::KwINTEGER,
                       asn1::TokenKind::KwBOOLEAN,
                       asn1::TokenKind::KwSEQUENCE,
                       asn1::TokenKind::KwCHOICE,
                       asn1::TokenKind::KwCLASS,
                       asn1::TokenKind::EndOfFile,
                   }));
}

TEST(Lexer, AssignmentRangeEllipsisAndBrackets) {
  asn1::Diagnostics diag;
  auto result = lex_all("::= .. ... [[ ]] { } [ ] ( )", diag);
  auto kinds = kinds_of(result.tokens);
  EXPECT_EQ(kinds, (std::vector<asn1::TokenKind>{
                       asn1::TokenKind::Assign,
                       asn1::TokenKind::Range,
                       asn1::TokenKind::Ellipsis,
                       asn1::TokenKind::VersionLBracket,
                       asn1::TokenKind::VersionRBracket,
                       asn1::TokenKind::LBrace,
                       asn1::TokenKind::RBrace,
                       asn1::TokenKind::LBracket,
                       asn1::TokenKind::RBracket,
                       asn1::TokenKind::LParen,
                       asn1::TokenKind::RParen,
                       asn1::TokenKind::EndOfFile,
                   }));
}

TEST(Lexer, NumberVsRangeVsReal) {
  asn1::Diagnostics diag;
  auto result = lex_all("0..255 1.5 2E10", diag);
  auto kinds = kinds_of(result.tokens);
  EXPECT_EQ(kinds, (std::vector<asn1::TokenKind>{
                       asn1::TokenKind::Number,
                       asn1::TokenKind::Range,
                       asn1::TokenKind::Number,
                       asn1::TokenKind::RealNumber,
                       asn1::TokenKind::RealNumber,
                       asn1::TokenKind::EndOfFile,
                   }));
  EXPECT_EQ(result.tokens[0].text, "0");
  EXPECT_EQ(result.tokens[2].text, "255");
  EXPECT_EQ(result.tokens[3].text, "1.5");
  EXPECT_EQ(result.tokens[4].text, "2E10");
}

TEST(Lexer, SkipsLineAndInlineComments) {
  asn1::Diagnostics diag;
  auto result = lex_all("A -- comment\nB --c-- C", diag);
  auto kinds = kinds_of(result.tokens);
  EXPECT_EQ(kinds, (std::vector<asn1::TokenKind>{
                       asn1::TokenKind::TypeReference,
                       asn1::TokenKind::TypeReference,
                       asn1::TokenKind::TypeReference,
                       asn1::TokenKind::EndOfFile,
                   }));
  EXPECT_EQ(result.tokens[0].text, "A");
  EXPECT_EQ(result.tokens[1].text, "B");
  EXPECT_EQ(result.tokens[2].text, "C");
}

TEST(Lexer, HyphenatedNamesAndCommentBoundary) {
  asn1::Diagnostics diag;
  auto result = lex_all("RELATIVE-OID my-value--comment\nX", diag);
  ASSERT_GE(result.tokens.size(), 3u);
  EXPECT_EQ(result.tokens[0].kind, asn1::TokenKind::KwRELATIVE_OID);
  EXPECT_EQ(result.tokens[1].kind, asn1::TokenKind::Identifier);
  EXPECT_EQ(result.tokens[1].text, "my-value");
  EXPECT_EQ(result.tokens[2].kind, asn1::TokenKind::TypeReference);
  EXPECT_EQ(result.tokens[2].text, "X");
}

TEST(Lexer, BinaryHexAndCharacterStrings) {
  asn1::Diagnostics diag;
  auto result = lex_all("'0101'B 'A B'H \"he\"\"llo\"", diag);
  auto kinds = kinds_of(result.tokens);
  EXPECT_EQ(kinds, (std::vector<asn1::TokenKind>{
                       asn1::TokenKind::BinaryString,
                       asn1::TokenKind::HexString,
                       asn1::TokenKind::CharacterString,
                       asn1::TokenKind::EndOfFile,
                   }));
  EXPECT_EQ(result.tokens[2].text, "\"he\"\"llo\"");
  EXPECT_TRUE(diag.ok());
}

TEST(Lexer, ModuleSkeletonTokenStream) {
  const char* src = R"(
MyModule DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Person ::= SEQUENCE {
    id   INTEGER (0..65535),
    name UTF8String,
    age  INTEGER (0..150) OPTIONAL
  }
END
)";
  asn1::Diagnostics diag;
  auto result = lex_all(src, diag);
  EXPECT_TRUE(diag.ok());
  EXPECT_EQ(result.tokens.front().kind, asn1::TokenKind::TypeReference);
  EXPECT_EQ(result.tokens.front().text, "MyModule");

  // Spot-check a few important tokens exist in order.
  auto kinds = kinds_of(result.tokens);
  auto contains_seq = [&](std::initializer_list<asn1::TokenKind> needle) {
    auto it = kinds.begin();
    for (asn1::TokenKind k : needle) {
      it = std::find(it, kinds.end(), k);
      if (it == kinds.end()) {
        return false;
      }
      ++it;
    }
    return true;
  };

  EXPECT_TRUE(contains_seq({asn1::TokenKind::KwDEFINITIONS,
                            asn1::TokenKind::KwAUTOMATIC,
                            asn1::TokenKind::KwTAGS,
                            asn1::TokenKind::Assign,
                            asn1::TokenKind::KwBEGIN}));
  EXPECT_TRUE(contains_seq({asn1::TokenKind::KwSEQUENCE,
                            asn1::TokenKind::LBrace,
                            asn1::TokenKind::Identifier,
                            asn1::TokenKind::KwINTEGER,
                            asn1::TokenKind::LParen,
                            asn1::TokenKind::Number,
                            asn1::TokenKind::Range,
                            asn1::TokenKind::Number}));
  EXPECT_TRUE(contains_seq({asn1::TokenKind::KwOPTIONAL,
                            asn1::TokenKind::RBrace,
                            asn1::TokenKind::KwEND,
                            asn1::TokenKind::EndOfFile}));
}

TEST(Lexer, ReportsUnexpectedCharacterWithLocation) {
  asn1::Diagnostics diag;
  auto result = lex_all("A $ B", diag, "bad.asn");
  EXPECT_FALSE(diag.ok());
  ASSERT_FALSE(diag.items().empty());
  EXPECT_EQ(diag.items().front().where.begin.file, "bad.asn");
  EXPECT_EQ(diag.items().front().where.begin.line, 1);
  EXPECT_NE(diag.items().front().where.begin.column, 0);

  bool saw_invalid = false;
  for (const auto& t : result.tokens) {
    if (t.kind == asn1::TokenKind::Invalid) {
      saw_invalid = true;
    }
  }
  EXPECT_TRUE(saw_invalid);
}
