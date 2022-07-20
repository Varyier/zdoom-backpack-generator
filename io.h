
#ifndef _BACKPACK_GEN_IO_H_
#define _BACKPACK_GEN_IO_H_

#include "common.h"

#include <vector>
#include <unordered_map>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Basic I/O, InStreamWithBuffer - input stream wrapper that reads buffer by buffer from this stream

namespace BpkGen {
	namespace Io {
		// input stream interface
		typedef std::basic_istream< char, std::char_traits<char> > InStream;

		// output stream interface
		typedef std::basic_ostream< char, std::char_traits<char> > OutStream;

		// InStreamWithBuffer - reads from underlying stream buffer by buffer
		class InStreamWithBuffer {
		public:
			// wraps the stream object
			InStreamWithBuffer(InStream& stream)
				: m_stream(stream)
				, m_line_number(1)
				, m_loaded_buffers_count(0)
				, m_buffer_index(0)
				, m_buffer_pos(0)
			{}

			// returns the char the pointer is currently located at;
			// call right after initialization returns first char
			// in the underlying stream (if have any);
			// throws on end of stream and if an error occured in a call to
			// the underlying stream
			char GetCurChar();

			// returns the char that's located right after the pointer;
			// throws if an error occured in a call to the underlying stream;
			// returns true - char exists, its value is stored in 'c',
			//         false - char does not exist (end of stream), 'c' is undefined
			bool TryGetNextChar(char& c);

			// returns the char located 'char_index' chars after the pointer;
			// throws if 'char_index' is greater than 'MAX_CHARS_TO_LOOK_AHEAD' or
			// if an error occured in a call to the underlying stream;
			// returns: true - char exists, its value is stored in 'c',
			//          false - char does not exist (end of stream), 'c' is undefined
			bool TryGetCharAhead(size_t char_index, char& c);

			// moves the pointer to the next char in the underlying stream;
			// throws if an error occured in a call to the underlying stream;
			// returns: true - have next char, pointer stays on it,
			//          false - end of stream, pointer is at end of stream
			bool MoveToNextChar();

			// moves the pointer to the char located 'char_index'
			// chars after the current position;
			// throws if an error occured in a call to the underlying stream;
			// returns: true - have char at 'char_index', pointer stays on it,
			//          false - end of stream, pointer is at end of stream
			bool MoveToCharAhead(size_t char_index);

			// reports about end of stream;
			// throws if an error occured in a call to the underlying stream;
			// returns: true - end of stream, the pointer's target is invalid
			//          false - not end of stream, the pointer points to a char 
			bool IsEndOfStream();

			// returns the line number where pointer's target is located
			// lines are numbered from 1, returns the last line number for end of stream
			size_t GetLineNumber();

		public:
			static const size_t BUFFERS_COUNT = 2;
			static const size_t BUFFER_SIZE_BYTES = 64 * 1024;
			static const size_t MAX_CHARS_TO_LOOK_AHEAD = BUFFER_SIZE_BYTES * (BUFFERS_COUNT-1);

		private:
			bool TryGetCharLocation(size_t chars_ahead, size_t& buffer_index, size_t& index);

			bool FetchMoreChars(CharArray& buffer);

			size_t CountLinesTo(size_t buffer_index, size_t char_index);

		private:
			InStream& m_stream;

			size_t m_line_number;

			CharArray m_buffers[BUFFERS_COUNT];
			size_t m_loaded_buffers_count;
			size_t m_buffer_index;
			size_t m_buffer_pos;
		};
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConfigData serialization format

namespace BpkGen {
	namespace Io {
		// config data entry - one matches each appearance of '#' in the input
		struct ConfigDataEntry {
			struct Field {
				enum EnType {
					enType_Undefined = 0,
					enType_Int,
					enType_String
				};

				std::string name;
				EnType field_type;
				int value_int;
				std::string value_string;

				Field(const std::string& name_ = "")
					: name(name_)
					, field_type(enType_Undefined)
					, value_int(0)
				{}

				Field(const std::string& name_, int value)
					: name(name_)
					, field_type(enType_Int)
					, value_int(value)
				{}

				Field(const std::string& name_, const std::string& value)
					: name(name_)
					, field_type(enType_String)
					, value_int(0)
					, value_string(value)
				{}
			};

			typedef std::vector<Field> FieldArray;

			std::string name;
			FieldArray fields;

			ConfigDataEntry(const std::string& name_ = "")
				: name(name_)
			{}
		};

		// config data entry list
		typedef std::vector<ConfigDataEntry> ConfigDataEntryArray;

		// config data: entry list type -> entry list
		typedef std::unordered_map<std::string, ConfigDataEntryArray> ConfigData;

		// read formatted data from the stream, throw exception in case of bad format
		void ReadConfigFromStream(InStream& stream, ConfigData& ammo_config);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PipeCharsUntilNextBpkGenLabel - copies chars from the input stream to the output stream, stops after first
// @BpkGen<Tail> label (not escaped with @ in the head) found and returns true with label_content=<Tail>
// OR on end of stream (returns false);
// <Tail> may be only an identifier-like string '[_a-zA-Z0-9\-]+' that may be followed by '('
// - in this case all chars until ')' (including it) are included
// in the return value (escape sequences in this case are "\) -> )" and "\\ -> \";
// In case there are no '(' in the tail, it may be terminated with @, so text "@BpkGenL1@tail" will stop on t after second
// @ and return L1

namespace BpkGen {
	namespace Io {
		bool PipeCharsUntilNextBpkGenLabel(InStreamWithBuffer& in_stream_with_buffer,
										   OutStream& out_stream,
										   std::string& label_content,
										   std::string& indent);

		enum EnBadBpkGenLabel {
			// no label error
			enBadLabel_None = 0,

			// bad label param text
			enBadLabel_BadParams,

			// unknown label
			enBadLabel_Unknown,

			// label should exist, but it is missing
			enBadLabel_Missing,

			// disallowed label duplicate
			// (some labels are allowed to have dups)
			enBadLabel_Duplicated,

			// label placed in the wrong place
			enBadLabel_Misplaced,

			// label is redundant and should be removed
			enBadLabel_Redundant
		};

		// format error message about bad '@BpkGen' label and report it
		// (via exception or logging)
		void ReportBpkGenLabelError(EnBadBpkGenLabel bad_label_type,
									const std::string& label_name_tail,
									const std::string& label_source,
									size_t line_number,
									const std::string& details,
									bool throw_out,
									OutStream* log_stream);

		// format error message about bad '@BpkGen' label
		std::string GetBadBpkGenLabelMessage(EnBadBpkGenLabel bad_label_type,
											 const std::string& label_name_tail,
											 const std::string& label_source,
											 size_t line_number,
											 const std::string& details);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WadWriter - class, that write formatted WAD file

namespace BpkGen {
	namespace Io {
		class WadWriter {
		public:
			WadWriter(OutStream& stream, bool is_iwad=false)
				: m_stream(stream)
				, m_is_iwad(is_iwad)
				, m_start_pos(-1)
				, m_dir_info_off(-1)
				, m_writing_started(false)
				, m_writing_finished(false)
				, m_writing_lump(false)
				, m_lump_start_off(-1)
			{}

			~WadWriter() {
				FinishWriting();
			}

			// set true - IWAD, false - PWAD;
			// can set only before writing
			void SetIsIwad(bool is_iwad);

			// check if string is valid WAD lump name
			static bool IsValidLumpName(const std::string& lump_name);

			// start new lump writing (end any previous one writing);
			// return stream that lump should be written into
			OutStream& StartLumpWriting(const std::string& lump_name);

			// finish writing WAD file
			void FinishWriting();

			// reset to the initial state - can write new WAD file
			void Reset();

		private:
			std::streamoff FinishLumpWriting();

		private:
			OutStream& m_stream;
			bool m_is_iwad;
			std::streampos m_start_pos;
			std::streamoff m_dir_info_off;

			bool m_writing_started;
			bool m_writing_finished;

			struct DirectoryEntry {
				long start_pos;
				long size;
				char name[8];

				DirectoryEntry()
					: start_pos(-1)
					, size(-1)
				{
					std::fill(name, name+8, (char)'\0');
				}
			};

			typedef std::vector<DirectoryEntry> DirectoryEntryArray;
			DirectoryEntryArray m_directory;

			bool m_writing_lump;
			std::string m_lump_name;
			std::streamoff m_lump_start_off;
		};
	}
}

#endif // _BACKPACK_GEN_IO_H_
