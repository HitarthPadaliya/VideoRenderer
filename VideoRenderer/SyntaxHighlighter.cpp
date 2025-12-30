#include "SyntaxHighlighter.h"
#include <cwctype>


SyntaxHighlighter::SyntaxHighlighter(Slide* pSlide) : m_pSlide(pSlide) {}


std::vector<Token> SyntaxHighlighter::Tokenize()
{
	std::vector<Token> tokens;
	while (true)
	{
		Token token = Next();
		tokens.push_back(token);
		if (token.type == TokenType::EndOfFile)
			break;
	}

	return tokens;
}

Token SyntaxHighlighter::Next()
{
	SkipWhitespace();

	if (IsEOF())
		return MakeToken(TokenType::EndOfFile, m_Position, m_Position);

	if (Peek() == '/' && Peek(1) == '/')
		return LexLineComment();
	if (Peek() == '/' && Peek(1) == '*')
		return LexBlockComment();

	if (Peek() == '#' && m_bOnlyWhitespaceSinceBol)
	{
		uint32_t sp = m_Position;
		Advance();
		m_bInPreprocessorLine = true;
		m_bAfterHash = true;

		return MakeToken(TokenType::Preprocessor, sp, m_Position);
	}

	if (StartsStringLiteral())
		return LexStringLiteral();
	if (StartsCharLiteral())
		return LexCharLiteral();

	if (m_bExpectIncludePath && Peek() == '<')
		return LexAngleIncludeString();

	if (std::iswalpha(Peek()) || Peek() == '_')
		return LexIdentifierFamily();

	if (std::iswdigit(Peek()))
		return LexNumber();

	return LexOther();
}


std::wstring SyntaxHighlighter::GetTokenTypeName(const Token& token)
{
	switch (token.type)
	{
		case TokenType::Function:			return L"Function";
		case TokenType::Class:				return L"Class";
		case TokenType::EnumVal:			return L"EnumVal";
		case TokenType::Parameter:			return L"Parameter";
		case TokenType::LocalVar:			return L"LocalVar";
		case TokenType::MemberVar:			return L"MemberVar";

		case TokenType::Keyword:			return L"Keyword";
		case TokenType::ControlStatement:	return L"ControlStatement";
		case TokenType::Preprocessor:		return L"Preprocessor";
		case TokenType::Comment:			return L"Comment";
		case TokenType::Macro:				return L"Macro";
		case TokenType::UEMacro:			return L"UEMacro";

		case TokenType::Number:				return L"Number";
		case TokenType::StringLiteral:		return L"StringLiteral";
		case TokenType::CharLiteral:		return L"CharLiteral";

		case TokenType::EndOfFile:			return L"EndOfFile";

		default:							return L"Other";
	}
}

Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> SyntaxHighlighter::GetBrush(const Token& token)
{
	switch (token.type)
	{
		case TokenType::Function:			return Brushes::Function;
		case TokenType::Class:				return Brushes::Class;
		case TokenType::EnumVal:			return Brushes::EnumVal;
		case TokenType::Parameter:			return Brushes::Parameter;
		case TokenType::LocalVar:			return Brushes::LocalVar;
		case TokenType::MemberVar:			return Brushes::MemberVar;

		case TokenType::Keyword:			return Brushes::Keyword;
		case TokenType::ControlStatement:	return Brushes::ControlStatement;
		case TokenType::Preprocessor:		return Brushes::Preprocessor;
		case TokenType::Comment:			return Brushes::Comment;
		case TokenType::Macro:				return Brushes::Macro;
		case TokenType::UEMacro:			return Brushes::UEMacro;

		case TokenType::Number:				return Brushes::Number;
		case TokenType::StringLiteral:		return Brushes::StringLiteral;
		case TokenType::CharLiteral:		return Brushes::CharLiteral;

		default:							return Brushes::Other;
	}
}


bool SyntaxHighlighter::IsEOF(const uint32_t& offset) const
{
	return m_Position + offset >= m_pSlide->m_Code.size();
}


wchar_t SyntaxHighlighter::Peek(const uint32_t& offset) const
{
	return IsEOF(offset) ? '\0' : m_pSlide->m_Code[m_Position + offset];
}

wchar_t SyntaxHighlighter::Advance()
{
	wchar_t c = Peek();
	++m_Position;

	if (c == '\n')
	{
		m_bOnlyWhitespaceSinceBol	= true;
		m_bInPreprocessorLine		= false;
		m_bAfterHash				= false;
		m_bAfterDefine				= false;
		m_bExpectIncludePath		= false;
	}
	else if (c != ' ')
	{
		m_bOnlyWhitespaceSinceBol = false;
	}

	return c;
}

void SyntaxHighlighter::SkipWhitespace()
{
	while (!IsEOF() && Peek() == ' ')
		Advance();
}


Token SyntaxHighlighter::MakeToken(const enum class TokenType& type,
	const uint32_t& start, const uint32_t& end) const
{
	Token token;
	token.type = type;
	token.start = start;
	token.length = (end >= start) ? (end - start) : 0;

	return token;
}


Token SyntaxHighlighter::LexLineComment()
{
	uint32_t sp = m_Position;
	Advance();
	Advance();
	while (!IsEOF() && Peek() != '\n')
		Advance();

	return MakeToken(TokenType::Comment, sp, m_Position);
}

Token SyntaxHighlighter::LexBlockComment()
{
	uint32_t sp = m_Position;
	Advance();
	Advance();
	while (!IsEOF())
	{
		if (Peek() == '*' && Peek(1) == '/')
		{
			Advance();
			Advance();
			break;
		}

		Advance();
	}

	return MakeToken(TokenType::Comment, sp, m_Position);
}


bool SyntaxHighlighter::StartsStringLiteral() const
{
	if (Peek() == '"')
		return true;
	if (Peek() == 'R' && Peek(1) == '"')
		return true;

	if (Peek() == 'u' && Peek(1) == '8')
	{
		if (Peek(2) == '"')
			return true;
		if (Peek(2) == 'R' && Peek(3) == '"')
			return true;
	}

	if ((Peek() == 'u' || Peek() == 'U' || Peek() == 'L') && Peek(1) == '"')
		return true;
	if ((Peek() == 'u' || Peek() == 'U' || Peek() == 'L') && (Peek(1) == 'R' && Peek(2) == '"'))
		return true;

	return false;
}

bool SyntaxHighlighter::StartsCharLiteral() const
{
	if (Peek() == '\'')
		return true;
	if ((Peek() == 'u' || Peek() == 'U' || Peek() == 'L') && Peek(1) == '\'')
		return true;
	return false;
}


Token SyntaxHighlighter::LexStringLiteral()
{
	uint32_t sp = m_Position;

	if (Peek() == 'u' && Peek(1) == '8')
	{
		Advance();
		Advance();
	}
	else if (Peek() == 'u' || Peek() == 'U' || Peek() == 'L')
	{
		Advance();
	}

	if (Peek() == 'R')
	{
		Advance();
		if (Peek() != '"')
			return MakeToken(TokenType::Other, sp, m_Position);
		Advance();

		std::wstring delim;
		while (!IsEOF() && Peek() != '(' && Peek() != '\n')
			delim.push_back(Advance());
		if (Peek() == '(')
			Advance();

		while (!IsEOF())
		{
			if (Peek() == '(')
			{
				uint32_t savePos = m_Position;
				Advance();
				bool ok = true;

				for (const wchar_t& c : delim)
				{
					if (Peek() != c)
					{
						ok = false;
						break;
					}
				}

				if (ok && Peek() == '"')
				{
					Advance();
					break;
				}

				m_Position = savePos;
			}

			Advance();
		}

		return MakeToken(TokenType::StringLiteral, sp, m_Position);
	}

	if (Peek() != '"')
		return MakeToken(TokenType::Other, sp, m_Position);
	Advance();

	while (!IsEOF())
	{
		wchar_t c = Advance();
		if (c == '\\')
		{
			if (!IsEOF())
				Advance();
		}
		else if (c == '"')
		{
			break;
		}
		else if (c == '\n')
		{
			break;
		}
	}

	return MakeToken(TokenType::StringLiteral, sp, m_Position);
}

Token SyntaxHighlighter::LexCharLiteral()
{
	uint32_t sp = m_Position;

	if (Peek() == 'u' || Peek() == 'U' || Peek() == 'L')
		Advance();
	if (Peek() != '\'')
		return MakeToken(TokenType::Other, sp, m_Position);

	Advance();

	while (!IsEOF())
	{
		wchar_t c = Advance();
		if (c == '\\')
		{
			if (!IsEOF())
				Advance();
		}
		else if (c == '\'')
		{
			break;
		}
		else if (c == '\n')
		{
			break;
		}
	}

	return MakeToken(TokenType::CharLiteral, sp, m_Position);
}

Token SyntaxHighlighter::LexAngleIncludeString()
{
	uint32_t sp = m_Position;
	Advance();

	while (!IsEOF())
	{
		wchar_t c = Advance();
		if (c == '>')
			break;
		if (c == '\n')
			break;
	}

	m_bExpectIncludePath = false;
	return MakeToken(TokenType::StringLiteral, sp, m_Position);
}


Token SyntaxHighlighter::LexIdentifierFamily()
{
	uint32_t sp = m_Position;
	Advance();

	while (!IsEOF())
	{
		wchar_t c = Peek();
		if (std::iswalnum(c) || c == '_')
			Advance();
		else
			break;
	}

	std::wstring lex = m_pSlide->m_Code.substr(sp, m_Position - sp);

	if (m_bInPreprocessorLine && m_bAfterHash)
	{
		m_bAfterHash = false;

		if (m_Preprocessor.count(lex))
		{
			if (lex == L"define")
				m_bAfterDefine = true;
			if (lex == L"include")
				m_bExpectIncludePath = true;

			return MakeToken(TokenType::Preprocessor, sp, m_Position);
		}
	}

	if (m_bInPreprocessorLine && m_bAfterDefine)
	{
		m_bAfterDefine = false;
		if (m_UEMacros.count(lex))
			return MakeToken(TokenType::UEMacro, sp, m_Position);
		return MakeToken(TokenType::UEMacro, sp, m_Position);
	}

	if (m_UEMacros.count(lex))
		return MakeToken(TokenType::UEMacro, sp, m_Position);

	if (m_Keywords.count(lex))
		return MakeToken(TokenType::Keyword, sp, m_Position);
	if (m_ControlStatements.count(lex))
		return MakeToken(TokenType::ControlStatement, sp, m_Position);

	if (IsMacroLike(lex))
		return MakeToken(TokenType::Macro, sp, m_Position);
	if (IsClassLike(lex))
		return MakeToken(TokenType::Class, sp, m_Position);

	if (m_pSlide->m_Functions.count(lex))
		return MakeToken(TokenType::Function, sp, m_Position);
	if (m_pSlide->m_Params.count(lex))
		return MakeToken(TokenType::Parameter, sp, m_Position);
	if (m_pSlide->m_LocalVars.count(lex))
		return MakeToken(TokenType::LocalVar, sp, m_Position);
	if (m_pSlide->m_MemberVars.count(lex))
		return MakeToken(TokenType::MemberVar, sp, m_Position);

	return MakeToken(TokenType::EnumVal, sp, m_Position);
}

Token SyntaxHighlighter::LexNumber()
{
	uint32_t sp = m_Position;

	auto IsDigit = [](wchar_t c) { return std::iswdigit(c); };

	while (!IsEOF() && IsDigit(Peek()))
		Advance();

	if (!IsEOF() && Peek() == '.' && IsDigit(Peek(1)))
	{
		Advance();
		while (!IsEOF() && IsDigit(Peek()))
			Advance();
	}

	if (!IsEOF() && (Peek() == 'e' || Peek() == 'E'))
	{
		Advance();
		if (!IsEOF() && (Peek() == '+' || Peek() == '-'))
			Advance();
		while (!IsEOF() && IsDigit(Peek()))
			Advance();
	}

	while (!IsEOF() && std::iswalpha(Peek()))
		Advance();

	return MakeToken(TokenType::Number, sp, m_Position);
}

Token SyntaxHighlighter::LexOther()
{
	uint32_t sp = m_Position;

	auto Match2 = [&](wchar_t a, wchar_t b)
	{
		if (Peek() == a && Peek(1) == b)
		{
			Advance();
			Advance();
			return true;
		}

		return false;
	};

	if
	(
		Match2(':', ':') || Match2('-', '>') || Match2('+', '+') || Match2('-', '-') ||
		Match2('=', '=') || Match2('!', '=') || Match2('<', '=') || Match2('>', '=') ||
		Match2('&', '&') || Match2('|', '|') || Match2('+', '=') || Match2('-', '=') ||
		Match2('*', '=') || Match2('/', '=') || Match2('%', '=') || Match2('&', '=') ||
		Match2('|', '=') || Match2('^', '=') || Match2('<', '<') || Match2('>', '>')
	)
	{
		return MakeToken(TokenType::Other, sp, m_Position);
	}

	Advance();
	return MakeToken(TokenType::Other, sp, m_Position);
}


bool SyntaxHighlighter::IsMacroLike(const std::wstring& s)
{
	if (s.empty())
		return false;
	if (!(std::isupper(static_cast<unsigned>(s[0])) || s[0] == '_'))
		return false;

	for (const wchar_t& c : s)
	{
		if (!(std::isupper(static_cast<unsigned>(c)) || std::iswdigit(static_cast<unsigned>(c)) || c == '_'))
			return false;
	}

	return true;
}

bool SyntaxHighlighter::IsUETypePrefix(const wchar_t& c)
{
	switch (c)
    {
        case L'A': // AActor-derived
        case L'U': // UObject-derived
        case L'F': // most other types/struct-like
        case L'S': // Slate widgets
        case L'I': // interfaces
        case L'E': // enums
        case L'T': // templates
        case L'C': // used in Epic docs/examples for certain concept-alike structs
            return true;
        default:
            return false;
    }
}

bool SyntaxHighlighter::IsClassLike(const std::wstring& s)
{
	if (s.size() < 2)
		return false;
	if (!IsUETypePrefix(s[0]))
		return false;
	if (!std::iswupper(s[1]))
		return false;

	for (const wchar_t& c : s)
	{
		if (!std::iswalnum(c))
			return false;
	}

	bool hasAlphaAfterPrefix = false;
	for (uint32_t i = 1; i < s.size(); ++i)
	{
		if (std::iswalpha(s[i]))
		{
			hasAlphaAfterPrefix = true;
			break;
		}
	}

	if (!hasAlphaAfterPrefix)
		return false;

	return true;
}
