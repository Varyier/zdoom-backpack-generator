
#ifndef _BACKPACK_GEN_DECR_H_
#define _BACKPACK_GEN_DECR_H_

#define SDECR_TRAILING_ENDL() (_decr_trailing_endl_ ? "\n" : "")

#define DECR_START(stream) { Io::OutStream& _decr_stream_ = stream; const char* _decr_spcs_ = ""; bool _decr_trailing_endl_ = true; (void)_decr_stream_; (void)_decr_spcs_; (void)_decr_trailing_endl_;
#define DECR_END() }
#define DECR_ENABLE_TRAILING_ENDL() _decr_trailing_endl_ = true;
#define DECR_DISABLE_TRAILING_ENDL() _decr_trailing_endl_ = false;

#define DECR_TEXT(text) _decr_stream_ << (text);
#define DECR_TEXT_L(text) _decr_stream_ << (text) << SDECR_TRAILING_ENDL();
#define DECR_TEXT_S(text) _decr_stream_ << _decr_spcs_ << (text);
#define DECR_TEXT_SL(text) _decr_stream_ << _decr_spcs_ << (text) << SDECR_TRAILING_ENDL();
#define DECR_COMMENTED_CODE_L(code) _decr_stream_ << "//" << (code) << SDECR_TRAILING_ENDL();
#define DECR_COMMENTED_CODE_SL(text) _decr_stream_ << _decr_spcs_ << "//" << (text) << SDECR_TRAILING_ENDL();
#define DECR_LINE_COMMENT_L(text) _decr_stream_ << "// " << (text) << SDECR_TRAILING_ENDL();
#define DECR_LINE_COMMENT_SL(text) _decr_stream_ << _decr_spcs_ << "// " << (text) << SDECR_TRAILING_ENDL();
#define DECR_CONST_INT_DEF(name, value) _decr_stream_ << "const int " << (name) << " = " << (value) << ";" << SDECR_TRAILING_ENDL();
#define DECR_SPACE() _decr_stream_ << "  ";
#define DECR_ENDL() _decr_stream_ << "\n";

#define DECR_ACTOR_START(name, parent) _decr_stream_ << "actor " << (name) << ": " << (parent) << "\n" << "{" << SDECR_TRAILING_ENDL(); _decr_spcs_ = "  ";
#define DECR_ACTOR_START_N(name, parent, ednum) _decr_stream_ << "actor " << (name) << ": " << (parent) << " " << (ednum) << "\n" << "{" << SDECR_TRAILING_ENDL(); _decr_spcs_ = "  ";
#define DECR_ACTOR_START_R(name, parent, to_replace) _decr_stream_ << "actor " << (name) << ": " << (parent) << " replaces " << (to_replace) << "\n" << "{" << SDECR_TRAILING_ENDL(); _decr_spcs_ = "  ";
#define DECR_ACTOR_START_RN(name, parent, to_replace, ednum) _decr_stream_ << "actor " << (name) << ": " << (parent) << " replaces " << (to_replace) << " " << (ednum) << "\n" << "{" << SDECR_TRAILING_ENDL(); _decr_spcs_ = "  ";
#define DECR_ACTOR_END() _decr_stream_ << "}" << SDECR_TRAILING_ENDL() << SDECR_TRAILING_ENDL(); _decr_spcs_ = "";

#define DECR_ACTOR_PROP_NUM(name, num) _decr_stream_ << "  " << (name) << " " << (num) << SDECR_TRAILING_ENDL();
#define DECR_ACTOR_PROP_STR(name, str) _decr_stream_ << "  " << (name) << " \"" << (str) << "\"" << SDECR_TRAILING_ENDL();

#define DECR_ACTOR_STATES_START() _decr_stream_ << "  States" << "\n" << "  {" << SDECR_TRAILING_ENDL();
#define DECR_ACTOR_STATES_END() _decr_stream_ << "  }" << SDECR_TRAILING_ENDL();
#define DECR_ACTOR_STATE_LABEL(label) _decr_stream_ << "  " << (label) << ":" << SDECR_TRAILING_ENDL(); _decr_spcs_ = "    ";
#define DECR_ACTOR_STATE(sprite, letters, duration, function) _decr_stream_ << "    " << (sprite) << " " << (letters) << " " << (duration) << " " << (function) << SDECR_TRAILING_ENDL();
#define DECR_ACTOR_STATE_ENDSEQ(keyword) _decr_stream_ << "    " << (keyword) << SDECR_TRAILING_ENDL(); _decr_spcs_ = "  ";
#define DECR_ACTOR_STATE_GOTO(label) _decr_stream_ << "    Goto " << (label) << SDECR_TRAILING_ENDL(); _decr_spcs_ = "  ";
#define DECR_ACTOR_STATE_GOTO_O(label, offset) _decr_stream_ << "    Goto " << (label) << "+" << (offset) << SDECR_TRAILING_ENDL(); _decr_spcs_ = "  ";

#endif // _BACKPACK_GEN_DECR_H_
