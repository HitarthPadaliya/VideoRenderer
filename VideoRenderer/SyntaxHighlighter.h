#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <d2d1_1.h>
#include <wrl/client.h>

#include "Slide.h"


namespace Colors
{
	const D2D1::ColorF Function			(0.8627f, 0.8627f, 0.6667f, 1.0f);
	const D2D1::ColorF Class			(0.3059f, 0.7882f, 0.6902f, 1.0f);
	const D2D1::ColorF EnumVal			(0.7098f, 0.8078f, 0.6588f, 1.0f);
	const D2D1::ColorF Parameter		(0.6039f, 0.6039f, 0.6039f, 1.0f);
	const D2D1::ColorF LocalVar			(0.6118f, 0.8627f, 0.9961f, 1.0f);
	const D2D1::ColorF MemberVar		(0.8549f, 0.8549f, 0.8549f, 1.0f);

	const D2D1::ColorF Keyword			(0.3373f, 0.6118f, 0.8392f, 1.0f);
	const D2D1::ColorF ControlStatement	(0.8471f, 0.6275f, 0.8745f, 1.0f);
	const D2D1::ColorF Preprocessor		(0.6078f, 0.6078f, 0.6078f, 1.0f);
	const D2D1::ColorF Comment			(0.3411f, 0.6509f, 0.2902f, 1.0f);
	const D2D1::ColorF Macro			(0.7451f, 0.7176f, 1.0000f, 1.0f);
	//  const D2D1::ColorF UEMacro			(0.5690f, 0.6824f, 0.4683f, 1.0f);
	const D2D1::ColorF UEMacro			(0.7725f, 0.5255f, 0.7529f, 1.0f);

	const D2D1::ColorF Number			(0.7098f, 0.8078f, 0.6588f, 1.0f);
	const D2D1::ColorF StringLiteral	(0.8078f, 0.5686f, 0.4706f, 1.0f);
	const D2D1::ColorF CharLiteral		(0.8078f, 0.5686f, 0.4706f, 1.0f);

	const D2D1::ColorF Other			(0.7059f, 0.7059f, 0.7059f, 1.0f);
}

namespace Brushes
{
	inline Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>
		Function, Class, EnumVal, Parameter, LocalVar, MemberVar,
		Keyword, ControlStatement, Preprocessor, Comment, Macro, UEMacro,
		Number, StringLiteral, CharLiteral,
		Other;
}

enum class TokenType : uint8_t
{
	Function,
	Class,		// Also includes enum, struct, union
	EnumVal,
	Parameter,
	LocalVar,
	MemberVar,

	Keyword,
	ControlStatement,
	Preprocessor,
	Comment,
	Macro,
	UEMacro,

	Number,
	StringLiteral,
	CharLiteral,

	EndOfFile,
	Other
};

struct Token
{
	enum class TokenType type = TokenType::Other;
	uint32_t start = 0;
	uint8_t length = 0;

	std::wstring Text(const std::wstring& code) const
	{
		return code.substr(start, length);
	}
};


class SyntaxHighlighter
{
	private:
		uint32_t m_Position = 0;

		bool m_bOnlyWhitespaceSinceBol	= true;
		bool m_bInPreprocessorLine		= false;
		bool m_bAfterHash				= false;
		bool m_bAfterDefine				= false;
		bool m_bExpectIncludePath		= false;

		Slide* m_pSlide;


	public:
		explicit SyntaxHighlighter(Slide* pSlide);

		std::vector<Token> Tokenize();
		Token Next();

		static std::wstring GetTokenTypeName(const Token& token);
		static Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> GetBrush(const Token& token);


	private:
		bool IsEOF(const uint32_t& offset = 0) const;
		wchar_t Peek(const uint32_t& offset = 0) const;
		wchar_t Advance();
		void SkipWhitespace();

		Token MakeToken(const enum class TokenType& type, const uint32_t& start, const uint32_t& end) const;

		Token LexLineComment();
		Token LexBlockComment();

		bool StartsStringLiteral() const;
		bool StartsCharLiteral() const;

		Token LexStringLiteral();
		Token LexCharLiteral();
		Token LexAngleIncludeString();

		Token LexIdentifierFamily();
		Token LexNumber();
		Token LexOther();

		static bool IsMacroLike(const std::wstring& s);
		static bool IsUETypePrefix(const wchar_t& c);
		static bool IsClassLike(const std::wstring& s);


		std::unordered_set<std::wstring> m_Keywords =
		{
			L"alignas",
			L"alignof",
			L"asm",
			L"auto",
			L"class",
			L"const",
			L"constexpr",
			L"const_cast",
			L"decltype",
			L"delete",
			L"dynamic_cast",
			L"enum",
			L"explicit",
			L"export",
			L"extern",
			L"friend",
			L"inline",
			L"mutable",
			L"namespace",
			L"new",
			L"noexcept",
			L"operator",
			L"private",
			L"protected",
			L"public",
			L"register",
			L"reinterpret_cast",
			L"sizeof",
			L"static",
			L"static_assert",
			L"static_cast",
			L"struct",
			L"template",
			L"this",
			L"thread_local",
			L"typedef",
			L"typeid",
			L"typename",
			L"union",
			L"using",
			L"virtual",
			L"volatile",
			L"concept",
			L"requires",
			L"bool",
			L"char",
			L"char8_t",
			L"char16_t",
			L"char32_t",
			L"wchar_t",
			L"short",
			L"int",
			L"long",
			L"signed",
			L"unsigned",
			L"float",
			L"double",
			L"void",
			L"size_t",
			L"ptrdiff_t",
			L"nullptr_t",
			L"auto",
			L"int8_t",
			L"int16_t",
			L"int32_t",
			L"int64_t",
			L"uint8_t",
			L"uint16_t",
			L"uint32_t",
			L"uint64_t",
			L"int8",
			L"int16",
			L"int32",
			L"int64",
			L"uint8",
			L"uint16",
			L"uint32",
			L"uint64",
			L"true",
			L"false",
			L"nullptr"
		};

		std::unordered_set<std::wstring> m_ControlStatements =
		{
			L"if",
			L"else",
			L"switch",
			L"case",
			L"default",
			L"return",
			L"break",
			L"continue",
			L"for",
			L"while",
			L"do"
			L"try",
			L"catch",
			L"throw",
			L"goto",
			L"co_await",
			L"co_return",
			L"co_yield"
		};

		std::unordered_set<std::wstring> m_UEMacros =
		{
			L"UCLASS",
			L"UINTERFACE",
			L"USTRUCT",
			L"UENUM",
			L"UFUNCTION",
			L"UPARAM",
			L"UPROPERTY",
			L"BlueprintType",
			L"BlueprintCallable",
			L"BlueprintPure",
			L"BlueprintImplementableEvent",
			L"BlueprintNativeEvent",
			L"Category",
			L"EditAnywhere",
			L"VisibleAnywhere",
			L"BlueprintReadWrite",
			L"BlueprintReadOnly"
		};

		std::unordered_set<std::wstring> m_Preprocessor =
		{
			L"include",
			L"define",
			L"undef",
			L"if",
			L"ifdef",
			L"ifndef",
			L"elif",
			L"else",
			L"endif",
			L"line",
			L"error",
			L"warning",
			L"pragma",
			L"defined"
		};
};
