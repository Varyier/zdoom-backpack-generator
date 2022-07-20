
#include "io.h"

#include <unordered_set>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// InStreamWithBuffer implementation

namespace BpkGen {
	namespace Io {
		char InStreamWithBuffer::GetCurChar() {
			char c = '\0';
			if(!TryGetCharAhead(0, c)) {
				throw Exception("i/o error - end of file occured before a character is found");
			}

			return c;
		}

		bool InStreamWithBuffer::TryGetNextChar(char& c) {
			return TryGetCharAhead(1, c);
		}

		bool InStreamWithBuffer::TryGetCharAhead(size_t char_index, char& c) {
			size_t buffer_index = ~(size_t)0;
			size_t char_index_in_buffer = ~(size_t)0;
			if(!TryGetCharLocation(char_index, buffer_index, char_index_in_buffer)) {
				return false;
			}

			c = m_buffers[buffer_index][char_index_in_buffer];
			return true;
		}

		bool InStreamWithBuffer::MoveToNextChar() {
			return MoveToCharAhead(1);
		}

		bool InStreamWithBuffer::MoveToCharAhead(size_t char_index) {
			do {
				const size_t bounded_char_index = 
					(char_index > MAX_CHARS_TO_LOOK_AHEAD)
						? MAX_CHARS_TO_LOOK_AHEAD
						: char_index;

				size_t buffer_index = ~(size_t)0;
				size_t char_index_in_buffer = ~(size_t)0;
				if(!TryGetCharLocation(bounded_char_index, buffer_index, char_index_in_buffer)) {
					if(m_buffer_pos < m_buffers[m_buffer_index].size()) {
						buffer_index = (m_buffer_index + m_loaded_buffers_count - 1) % BUFFERS_COUNT;
						char_index_in_buffer = m_buffers[buffer_index].size();
						m_line_number += CountLinesTo(buffer_index, char_index_in_buffer);

						m_loaded_buffers_count = 1;
						m_buffer_index = 0;
						m_buffers[m_buffer_index].clear();
						m_buffer_pos = 0;
					}

					return false;
				}

				m_line_number += CountLinesTo(buffer_index, char_index_in_buffer);

				if(m_buffer_index != buffer_index) {
					// switch to right buffer
					m_loaded_buffers_count -=
						(buffer_index >= m_buffer_index)
							? (buffer_index - m_buffer_index)
							: (BUFFERS_COUNT - m_buffer_index + buffer_index);
					m_buffer_index = buffer_index;
				}

				m_buffer_pos = char_index_in_buffer;
				char_index -= bounded_char_index;
			} while(char_index != 0);

			return true;
		}

		bool InStreamWithBuffer::IsEndOfStream() {
			size_t buffer_index = ~(size_t)0;
			size_t char_index_in_buffer = ~(size_t)0;
			if(!TryGetCharLocation(0, buffer_index, char_index_in_buffer)) {
				return true;
			}

			return false;
		}

		size_t InStreamWithBuffer::GetLineNumber() {
			return m_line_number;
		}

		bool InStreamWithBuffer::TryGetCharLocation(size_t chars_ahead, size_t& buffer_index, size_t& index) {
			if(chars_ahead > MAX_CHARS_TO_LOOK_AHEAD) {
				throw Exception("i/o error - cannot get char located that far away");
			}

			if(m_loaded_buffers_count == 0) {
				m_loaded_buffers_count++;
				m_buffer_index = 0;
				m_buffer_pos = 0;
				if(!FetchMoreChars(m_buffers[0])) {
					// no more chars
					return false;
				}
			}

			buffer_index = m_buffer_index;
			index = m_buffer_pos + chars_ahead;
			if(index >= m_buffers[buffer_index].size()) {
				size_t preloaded_buffers_ahead = m_loaded_buffers_count - 1;
				while(index >= m_buffers[buffer_index].size()) {
					// move to next buffer
					if(preloaded_buffers_ahead != 0) {
						preloaded_buffers_ahead--;

					} else {
						const size_t new_buffer_index = (m_buffer_index + m_loaded_buffers_count) % BUFFERS_COUNT;
						if(!FetchMoreChars(m_buffers[new_buffer_index])) {
							// no more chars
							return false;
						}
						m_loaded_buffers_count++;
					}

					index -= m_buffers[buffer_index].size();
					buffer_index = (buffer_index + 1) % BUFFERS_COUNT;
				}
			}

			return true;
		}

		bool InStreamWithBuffer::FetchMoreChars(CharArray& buffer) {
			if(m_stream.eof()) {
				return false;
			}

			buffer.resize(BUFFER_SIZE_BYTES);
			bool have_exception = false;
			std::string exc_message;
			try {
				m_stream.read(buffer.data(), BUFFER_SIZE_BYTES);

			} catch(std::exception& e) {
				exc_message = e.what();
				if(!m_stream.bad() && !m_stream.fail() && !m_stream.eof()) {
					throw Exception("i/o unexpected error - " + exc_message);
				}

				have_exception = true;
			}

			if(m_stream.bad()) {
				if(!have_exception) {
					exc_message = "bad stream state";
				}
				throw Exception("i/o error - " + exc_message);

			} else if(m_stream.fail() && !m_stream.eof()) {
				if(!have_exception) {
					exc_message = "failed to read chars from stream";
				}
				throw Exception("i/o error - " + exc_message);

			} else if(m_stream.eof()) {
				// ok - eof
				const std::streamsize size_signed = m_stream.gcount();
				if(size_signed < 0) {
					throw Exception("i/o unexpected error - bad size of data read from stream: " + std::to_string(size_signed));
				}

				const size_t size = (size_t)size_signed;
				buffer.resize(size);
				if(size == 0) {
					return false;
				}

			} else {
				// successfully read enough chars - do nothing
			}

			return true;
		}

		size_t InStreamWithBuffer::CountLinesTo(size_t buffer_index, size_t char_index) {
			if(m_loaded_buffers_count == 0) {
				throw Exception("i/o error - attempted to read buffer before initialization");
			}

			// optimisation for the most often cases
			if(buffer_index == m_buffer_index) {
				if(char_index == 0) {
					return 0;

				} else if(char_index == 1) {
					if(    m_buffer_pos < m_buffers[m_buffer_index].size()
						&& m_buffers[m_buffer_index][m_buffer_pos] == '\n')
					{
						return 1;
					}

					return 0;
				}
			}

			size_t result = 0;
			size_t buffer_start_pos = m_buffer_pos;
			for(size_t bizb=0; bizb<m_loaded_buffers_count; bizb++) {
				const size_t bi = (m_buffer_index + bizb) % BUFFERS_COUNT;
				for(size_t ci=buffer_start_pos; ci<m_buffers[bi].size(); ci++) {
					if(bi == buffer_index && ci == char_index) {
						return result;
					}
					if(m_buffers[bi][ci] == '\n') {
						result++;
					}
				}

				if(bi == buffer_index && char_index == m_buffers[bi].size()) {
					return result;
				}

				buffer_start_pos = 0;
			}

			throw Exception("i/o error - cannot get line number after that many chars");
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ReadConfigFromStream implementation

namespace BpkGen {
	namespace Io {
		static bool ReadStringTokenCharByChar(InStreamWithBuffer& buf, std::string& token, size_t line_number);

		void ReadConfigFromStream(InStream& stream, ConfigData& ammo_config) {
			ammo_config.clear();

			const size_t max_chars_in_token = 2147483647;

			class CharUtils {
			public:
				bool IsNumberTokenFirstChar(char c) {
					return     (c >= '0' && c <= '9')
							|| c == '-';
				}

				bool IsTokenChar(char c) {
					return     (c >= '0' && c <= '9')
							|| (c >= 'a' && c <= 'z')
							|| (c >= 'A' && c <= 'Z')
							|| c == '-' || c == '_';
				}

				int ParseInt(const std::string& str, size_t line_number) {
					if(str.size() == 0) {
						throw Exception("format error - empty numeric value on line " + std::to_string(line_number));
					}

					int start_index = 0;
					bool negative = false;
					if(str[0] == '-') {
						if(str.size() == 1) {
							throw Exception("format error - bad numeric value '" + str + "' on line " + std::to_string(line_number));
						}

						negative = true;
						start_index++;
					}

					int result = 0;
					for(int cix=start_index; cix<str.size(); cix++) {
						if(str[cix] < '0' || str[cix] > '9') {
							throw Exception("format error - bad numeric value '" + str + "' on line " + std::to_string(line_number));
						}

						if(result >= 10000000) {
							throw Exception("format error - too big numeric absolute value '" + str + "' on line " + std::to_string(line_number));
						}

						result = result*10 + str[cix] - '0';
					}

					if(negative) {
						result = -result;
					}

					return result;
				}

				int ParseIntFromBoolLitOrNumber(const std::string& str, size_t line_number) {
					int value = 0;
					if(str == "false") {
						// zero

					} else if(str == "true") {
						value = 1;

					} else {
						// must be valid number
						value = ParseInt(str, line_number);
					}

					return value;
				}
			} char_utils;

			std::string entry_type;
			std::string entry_name;
			ConfigDataEntry* cur_entry_ptr = NULL;
			std::unordered_set<std::string> existing_prop_names;
			std::string entry_prop_name;

			std::string token;

			bool is_string_token = false;
			bool reading_token = false;

			bool skipping_line_comment = false;
			bool skipping_block_comment = false;

			bool reading_entry_data = false;
			bool reading_entry_data_prop_value = false;

			size_t line_number = 0;

			bool eof = false;
			for(InStreamWithBuffer stream_with_buffer(stream); !eof; stream_with_buffer.MoveToNextChar()) {
				char cur_char = ' ';
				if(stream_with_buffer.IsEndOfStream()) {
					// process the last token, if have any
					eof = true;

				} else {
					cur_char = stream_with_buffer.GetCurChar();
				}

				char next_char = '\0';

				// process comments
				if(skipping_block_comment) {
					if(cur_char == '*' && stream_with_buffer.TryGetNextChar(next_char) && next_char == '/') {
						stream_with_buffer.MoveToNextChar();
						skipping_block_comment = false;
					}
					continue;
				}

				const size_t new_line_number = stream_with_buffer.GetLineNumber();
				if(new_line_number != line_number) {
					skipping_line_comment = false;
					line_number = new_line_number;
					// process the first char on a new line

				} else {
					if(skipping_line_comment) {
						continue;
					}
				}

				// process current token
				if(reading_token && is_string_token) {
					if(ReadStringTokenCharByChar(stream_with_buffer, token, line_number)) {
						// continue reading string token
						continue;
					}

					reading_token = false;
					// double quote char is processed
					cur_char = ' ';

				} else if(char_utils.IsTokenChar(cur_char)) {
					if(!reading_token && !token.empty()) {
						// no two or more subsequent tokens allowed;
						// last token should have been processed
						throw Exception("format error - unexpected " + std::string(1, cur_char) + " after " + token + " on line " + std::to_string(line_number));
					}

					if(token.size() >= max_chars_in_token) {
						throw Exception("format error - token is too long on line " + std::to_string(line_number));
					}

					if(!reading_token) {
						reading_token = true;
						is_string_token = false;
					}

					token += cur_char;
					continue;
			
				} else {
					// a control or unknown char
					reading_token = false;
				}

				if(!token.empty() && reading_entry_data && !cur_entry_ptr) {
					if(entry_type.empty()) {
						throw Exception("format error - bad entry value on line " + std::to_string(line_number) + " - missing entry type");
					}

					ammo_config[entry_type].push_back(ConfigDataEntry(entry_name));
					ConfigDataEntry& entry = ammo_config[entry_type].back();
			
					if(is_string_token) {
						entry.fields.push_back(ConfigDataEntry::Field("", token));

					} else {
						entry.fields.push_back(ConfigDataEntry::Field("", char_utils.ParseIntFromBoolLitOrNumber(token, line_number)));
					}

					entry_name.clear();
					token.clear();
					reading_entry_data = false;
				}

				// process control chars
				switch(cur_char) {
				case '/':
					{
						if(stream_with_buffer.TryGetNextChar(next_char)) {
							if(next_char == '*') {
								skipping_block_comment = true;
								stream_with_buffer.MoveToNextChar();

							} else if(next_char == '/') {
								skipping_line_comment = true;
								stream_with_buffer.MoveToNextChar();

							} else {
								throw Exception("format error - unexpected '/' on line " + std::to_string(line_number));
							}

						} else {
							throw Exception("format error - unexpected '/' on line " + std::to_string(line_number));
						}
					}
					break;
			
				case '\"':
					{
						// a string token
						if(    !token.empty()
							|| !reading_entry_data
							|| (cur_entry_ptr && !reading_entry_data_prop_value))
						{
							throw Exception("format error - unexpected '\"' on line " + std::to_string(line_number));
						}

						reading_token = true;
						is_string_token = true;
					}
					break;

				case ' ':
				case '\t':
				case '\n':
				case '\r':
					// skip whitespace chars
					break;

				case ':':
					{
						// last read token is entry type marker and after goes entry list
						if(reading_entry_data) {
							throw Exception("format error - unexpected ':' in entry data on line " + std::to_string(line_number));
						}

						if(token.empty()) {
							if(entry_type.empty()) {
								throw Exception("format error - config entry type is not specified on line " + std::to_string(line_number));
							}
							// leave current entry type

						} else {
							// an entry list start marker
							if(char_utils.IsNumberTokenFirstChar(token[0])) {
								throw Exception("format error - config entry type should start with a letter or underscore, on line " + std::to_string(line_number));
							}
							entry_type = token;
						}

						token.clear();
					}
					break;
			
				case '#':
					{
						if(entry_type.empty() || reading_entry_data) {
							throw Exception("format error - unexpected '#' in entry data on line " + std::to_string(line_number));
						}

						if(!token.empty() && char_utils.IsNumberTokenFirstChar(token[0])) {
							throw Exception("format error - config entry name should start with a letter or underscore, on line " + std::to_string(line_number));
						}

						// may be empty
						entry_name = token;
						token.clear();
						reading_entry_data = true;
					}
					break;
			
				case '{':
					{
						if(!token.empty() || !reading_entry_data || cur_entry_ptr) {
							throw Exception("format error - unexpected '{' on line " + std::to_string(line_number));
						}

						ammo_config[entry_type].push_back(ConfigDataEntry(entry_name));
						cur_entry_ptr = &ammo_config[entry_type].back();
						entry_name.clear();
					}
					break;

				case '=':
					{
						if(token.empty() || !cur_entry_ptr || reading_entry_data_prop_value) {
							throw Exception("format error - unexpected '=' on line " + std::to_string(line_number));
						}

						if(is_string_token || char_utils.IsNumberTokenFirstChar(token[0])) {
							throw Exception("format error - unexpected string/number token on line " + std::to_string(line_number) + ", expected a field name");
						}

						entry_prop_name = token;
						reading_entry_data_prop_value = true;
						token.clear();
					}
					break;
			
				case ',':
					{
						if(token.empty() || !cur_entry_ptr || !reading_entry_data_prop_value) {
							throw Exception("format error - unexpected ',' on line " + std::to_string(line_number));
						}

						if(existing_prop_names.find(entry_prop_name) != existing_prop_names.end()) {
							throw Exception("format error - duplicated field '" + entry_prop_name + "' for an entry on line " + std::to_string(line_number));
						}
						existing_prop_names.insert(entry_prop_name);

						ConfigDataEntry& entry = *cur_entry_ptr;
						if(is_string_token) {
							entry.fields.push_back(ConfigDataEntry::Field(entry_prop_name, token));

						} else {
							entry.fields.push_back(ConfigDataEntry::Field(entry_prop_name, char_utils.ParseIntFromBoolLitOrNumber(token, line_number)));
						}

						entry_prop_name.clear();
						token.clear();
						reading_entry_data_prop_value = false;
					}
					break;

				case '}':
					{
						if(    !cur_entry_ptr
							|| (reading_entry_data_prop_value && token.empty())
							|| (!reading_entry_data_prop_value && !token.empty()))
						{
							throw Exception("format error - unexpected '}' on line " + std::to_string(line_number));
						}

						if(reading_entry_data_prop_value) {
							if(existing_prop_names.find(entry_prop_name) != existing_prop_names.end()) {
								throw Exception("format error - duplicated field '" + entry_prop_name + "' for an entry on line " + std::to_string(line_number));
							}
							existing_prop_names.insert(entry_prop_name);

							ConfigDataEntry& entry = *cur_entry_ptr;
							if(is_string_token) {
								entry.fields.push_back(ConfigDataEntry::Field(entry_prop_name, token));

							} else {
								entry.fields.push_back(ConfigDataEntry::Field(entry_prop_name, char_utils.ParseIntFromBoolLitOrNumber(token, line_number)));
							}

							entry_prop_name.clear();
							token.clear();
							reading_entry_data_prop_value = false;
						}

						reading_entry_data = false;
						cur_entry_ptr = NULL;
						existing_prop_names.clear();
					}
					break;
			
				default:
					throw Exception("format error - unexpected '" + std::string(1, cur_char) + "' on line " + std::to_string(line_number));
				}
			}

			if(skipping_block_comment) {
				throw Exception("format error - expected '*/' before end of file.");
			}

			if(cur_entry_ptr) {
				throw Exception("format error - expected '}' before end of file.");
			}

			if(reading_token && is_string_token) {
				throw Exception("format error - expected '\"' before end of file.");
			}

			if(!token.empty()) {
				throw Exception("unexpected " + token + " at end of file.");
			}
		}

		bool ReadStringTokenCharByChar(InStreamWithBuffer& stream_with_buffer, std::string& token, size_t line_number) {
			const size_t max_chars_in_token = 2147483647;
			// read everything found to the token until closing quote is found
			const char cur_char = stream_with_buffer.GetCurChar();
			char next_char = '\0';
			if(cur_char == '\\') {
				if(!stream_with_buffer.TryGetNextChar(next_char)) {
					throw Exception("format error - expected '\"' before end of file.");
				}

				switch(next_char) {
				case 't':
					next_char = '\t';
					break;

				case 'n':
					next_char = '\n';
					break;

				case 'r':
					next_char = '\r';
					break;

				case '\'':
				case '\"':
				case '\\':
					// add without backslash (escaping, just this style for now)
					break;

				default:
					throw Exception("format error - invalid escape sequence on line " + std::to_string(line_number));
				}

				if(token.size() >= max_chars_in_token) {
					throw Exception("format error - string token is too long on line " + std::to_string(line_number));
				}

				token += next_char;
				stream_with_buffer.MoveToNextChar();

			} else if(cur_char == '\"') {
				// the string token is read
				return false;

			} else {
				if(token.size() >= max_chars_in_token) {
					throw Exception("format error - string token is too long on line " + std::to_string(line_number));
				}

				token += cur_char;
			}

			// still reading
			return true;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PipeCharsUntilNextBpkGenLabel implementation

namespace BpkGen {
	namespace Io {
		bool PipeCharsUntilNextBpkGenLabel(InStreamWithBuffer& in_stream_with_buffer, OutStream& out_stream, std::string& label_content, std::string& indent) {
			class CharChecker {
			public:
				bool IsTokenChar(char c) {
					return    (c >= '0' && c <= '9')
							|| (c >= 'a' && c <= 'z')
							|| (c >= 'A' && c <= 'Z')
							|| c == '_' || c == '-';
				}
			} char_checker;

			label_content.clear();
			indent.clear();

			const std::string label_head = "@BpkGen";
			size_t label_head_index = 0;
			bool escape_sequence = false;
			bool reading_indent = true;
			bool reading_label_content = false;
			bool reading_complex_label = false;
			for( ; ; in_stream_with_buffer.MoveToNextChar()) {
				if(in_stream_with_buffer.IsEndOfStream()) {
					if(reading_complex_label) {
						const std::string complex_label_title = label_content.substr(0, label_content.find('('));
						throw Exception("bad template format - no closing parenthesis for label " + label_head + label_content);

					} else if(reading_label_content) {
						// end of label
						return true;

					} else {
						break;
					}
				}

				const char cur_char = in_stream_with_buffer.GetCurChar();
				if(reading_complex_label) {
					// reading a complex label @BpkGen<label-tail>(<data>)
					if(cur_char == '\\') {
						if(escape_sequence) {
							label_content += cur_char;
							escape_sequence = false;

						} else {
							escape_sequence = true;
						}

					} else {
						label_content += cur_char;
						if(cur_char == ')' && !escape_sequence) {
							// end of the label - return it
							//reading_complex_label = false;
							//reading_label_content = false;
							in_stream_with_buffer.MoveToNextChar();
							return true;
						}
						escape_sequence = false;
					}

				} else if(reading_label_content) {
					// reading after [^@]@BpkGen - the actual label text
					label_content += cur_char;
					if(cur_char == '(') {
						reading_complex_label = true;

					} else {
						char next_char = ' ';
						bool have_next_char = in_stream_with_buffer.TryGetNextChar(next_char);
						if(!have_next_char || (!char_checker.IsTokenChar(next_char) && next_char != '(')) {
							// end of label
							in_stream_with_buffer.MoveToNextChar();
							if(have_next_char && next_char == '@') {
								// skip at sign - explicit end of label
								in_stream_with_buffer.MoveToNextChar();
							}

							// return the label
							return true;
						}
					}

				} else {
					if(label_head_index > 0 && cur_char != label_head[label_head_index]) {
						// met just a part of the label start text - not a label, pipe it
						out_stream << label_head.substr(0, label_head_index);
						if(label_head_index == 1) {
							// just '@' sign that is followed by not the expected label char;
							// may be escaped label
							escape_sequence = true;

						} else {
							escape_sequence = false;
						}
						label_head_index = 0;
					}

					if(cur_char == label_head[label_head_index]) {
						label_head_index++;
						if(label_head_index == label_head.length()) {
							if(escape_sequence) {
								// escaped '@BpkGen' - not a real label
								out_stream << label_head.substr(1);
								escape_sequence = false;

							} else {
								reading_label_content = true;
								char next_char = ' ';
								if(!in_stream_with_buffer.TryGetNextChar(next_char) || !char_checker.IsTokenChar(next_char)) {
									throw Exception("bad template format - bad label on line " + std::to_string(in_stream_with_buffer.GetLineNumber()));
								}
							}
							label_head_index = 0;
						}
						reading_indent = false;

					} else {
						out_stream << cur_char;
						escape_sequence = false;

						if(cur_char == '\n' || cur_char == '\r') {
							indent.clear();
							reading_indent = true;

						} else if(reading_indent) {
							if(cur_char == ' ' || cur_char == '\t') {
								if(indent.size() < 1024) {
									indent += cur_char;
								}

							} else {
								reading_indent = false;
							}
						}
					}
				}
			}

			return false;
		}

		void ReportBpkGenLabelError(EnBadBpkGenLabel bad_label_type,
									const std::string& label_name_tail,
									const std::string& label_source,
									size_t line_number,
									const std::string& details,
									bool throw_out,
									OutStream* log_stream)
		{
			const std::string err_message = Io::GetBadBpkGenLabelMessage(bad_label_type, label_name_tail,
																		 label_source, line_number, details);
			if(throw_out) {
				throw Exception("bad " + (!label_source.empty() ? label_source + " " : "")  + "format - " + err_message);
			}

			if(log_stream) {
				(*log_stream) << err_message << std::endl;
			}
		}

		std::string GetBadBpkGenLabelMessage(EnBadBpkGenLabel bad_label_type,
											 const std::string& label_name_tail,
											 const std::string& label_source,
											 size_t line_number,
											 const std::string& details)
		{
			const std::string label_info_str = "label"
				+ (!label_name_tail.empty() ? (" '@BpkGen" + label_name_tail + "'") : "")
				+ (line_number > 0 ? (" on line " + std::to_string(line_number)) : "")
				+ (!label_source.empty() ? ((line_number > 0 ? " of " : " in ") + label_source) : "");
			const std::string details_tail_str = (!details.empty() ? (": " + details) : "");

			switch(bad_label_type) {
			case enBadLabel_BadParams:
				return "bad parameters for a " + label_info_str + details_tail_str;

			case enBadLabel_Unknown:
				return "unknown " + label_info_str + details_tail_str;

			case enBadLabel_Missing:
				return "missing " + label_info_str + details_tail_str;

			case enBadLabel_Duplicated:
				return "duplicated " + label_info_str + details_tail_str;

			case enBadLabel_Misplaced:
				return "misplaced " + label_info_str + details_tail_str;

			case enBadLabel_Redundant:
				return "redundant " + label_info_str + details_tail_str;

			case enBadLabel_None:
			default:
				break;
			}
			
			return "bad " + label_info_str + details_tail_str;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WadWriter implementation

namespace BpkGen {
	namespace Io {
		void WadWriter::SetIsIwad(bool is_iwad) {
			if(m_writing_started || m_writing_finished) {
				throw Exception("WAD writing error - WAD type can be changed only before writing has started");
			}

			m_is_iwad = is_iwad;
		}

		bool WadWriter::IsValidLumpName(const std::string& lump_name) {
			if(lump_name.empty() || lump_name.size() > 8) {
				return false;
			}

			// backslash is used for some Arch Vile sprites
			const std::string allowed_nonalphanum_chars("[]-_\\");
			for(size_t i=0; i<lump_name.size(); i++) {
				if(   (lump_name[i] >= 'A' && lump_name[i] <= 'Z')
				   || (lump_name[i] >= '0' && lump_name[i] <= '9')
				   || allowed_nonalphanum_chars.find(lump_name[i]) < lump_name.size())
				{
					// allowed char - check next
					continue;
				}

				return false;
			}

			return true;
		}

		OutStream& WadWriter::StartLumpWriting(const std::string& lump_name) {
			if(m_writing_finished) {
				throw Exception("WAD writing error - attempted to write a lump after WAD writing has ended");
			}

			if(!IsValidLumpName(lump_name)) {
				throw Exception("WAD writing error - bad lump name '" + lump_name + "'");
			}

			if(!m_writing_started) {
				m_start_pos = m_stream.tellp();
				// write first block of data (type, stubs for directory pos and size) - 12 bytes
				m_stream << (m_is_iwad ? "IWAD" : "PWAD");
				m_dir_info_off = m_stream.tellp() - m_start_pos;

				const std::string nulls4(4, '\0');
				m_stream << nulls4
						 << nulls4;
				m_writing_started = true;
			}

			const std::streamoff cur_off = FinishLumpWriting();
			m_lump_name = lump_name;
			m_lump_start_off = cur_off;
			m_writing_lump = true;

			return m_stream;
		}

		void WadWriter::FinishWriting() {
			class Utils {
			public:
				void PutLower4BytesToStream(OutStream& stream, long long number) {
					if(number < 0 || number > INT32_MAX) {
						throw Exception("WAD writing error - too big WAD file");
					}

					stream << (char)(number & 0xFF)
						   << (char)((number >> 8) & 0xFF)
						   << (char)((number >> 16) & 0xFF)
						   << (char)(number >> 24);
				}
			} utils;

			if(m_writing_finished) {
				return;
			}

			if(!m_writing_started) {
				// finished nothing
				return;
			}

			if(m_directory.size() > INT32_MAX) {
				throw Exception("WAD writing error - too many entries in WAD file");
			}

			const std::streamoff directory_off = FinishLumpWriting();

			// write the directory itself after all lumps
			for(size_t i=0; i<m_directory.size(); i++) {
				const DirectoryEntry& entry = m_directory[i];
				utils.PutLower4BytesToStream(m_stream, entry.start_pos);
				utils.PutLower4BytesToStream(m_stream, entry.size);
				for(size_t i=0; i<8; i++) {
					m_stream << entry.name[i];
				}
			}

			// write directory info to the beginning of the WAD
			const std::streampos post_end_pos = m_stream.tellp();
			m_stream.seekp(m_start_pos + m_dir_info_off);
			utils.PutLower4BytesToStream(m_stream, (long long)m_directory.size());
			utils.PutLower4BytesToStream(m_stream, directory_off);
			m_stream.seekp(post_end_pos);

			m_start_pos = -1;
			m_dir_info_off = -1;
			m_writing_started = false;
			m_writing_finished = true;
		}

		void WadWriter::Reset() {
			FinishWriting();
			m_writing_finished = false;
		}

		std::streamoff WadWriter::FinishLumpWriting() {
			const std::streampos cur_pos = m_stream.tellp();
			if(m_writing_lump) {
				// finish writing of the lump - add matching directory entry
				m_directory.push_back(DirectoryEntry());
				DirectoryEntry& entry = m_directory[m_directory.size()-1];
				m_lump_name.copy(entry.name, m_lump_name.length() > 8 ? 8 : m_lump_name.length());

				if(m_lump_start_off > INT32_MAX) {
					throw Exception("WAD writing error - too big WAD file");
				}
				entry.start_pos = (long)m_lump_start_off;

				const std::streamoff size = cur_pos - m_lump_start_off;
				if(size > INT32_MAX) {
					throw Exception("WAD writing error - too big WAD file");
				}
				entry.size = (long)size;

				m_lump_name.empty();
				m_lump_start_off = -1;
				m_writing_lump = false;
			}

			return cur_pos - m_start_pos;
		}
	}
}
