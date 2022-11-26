/*==============================================================================================================/
|                   _         _                  _   ___ _   _ ___ ____                                         |
|                  | |    ___| | _____ _   _ ___( ) |_ _| \ | |_ _|  _ \ __ _ _ __ ___  ___ _ __                |
|                  | |   / _ \ |/ / __| | | / __|/   | ||  \| || || |_) / _` | '__/ __|/ _ \ '__|               |
|                  | |__|  __/   <\__ \ |_| \__ \    | || |\  || ||  __/ (_| | |  \__ \  __/ |                  |
|                  |_____\___|_|\_\___/\__, |___/   |___|_| \_|___|_|   \__,_|_|  |___/\___|_|                  |
|                                      |___/                                                                    |
/===============================================================================================================/
| All-in-one INI file parser                                                                                    |
| Provides convenient cross-platform class to load and save .INI files                                          |
| Extends classic INI-file format with                                                                          |
| -> arrays (comma (',') separated values: |val1, val2, val3|)                                                  |
| -> maps (declared as |key1:val1, key2:val2, ... , keyN:valN|)                                                 |
| -> nested sections (Section2 is considered child of Section1, if Section2 is defined as [Section1.Section2])  |
| -> file includes (use ";#include <file_path>" to include file with relative or full system path <file_path>)  |
| Please look in provided file ini-test/test1.ini for extended ini examples, in test_app.cpp for usage examples |
| Language: C++, STL used                                                                                       |
| Version:  1.1                                                                                                 |
/===============================================================================================================/
| Copyright (c) 2015 by Borovik Alexey
| MIT License
|
| Permission is hereby granted, free of charge, to any person obtaining a
| copy of this software and associated documentation files (the "Software"),
| to deal in the Software without restriction, including without limitation
| the rights to use, copy, modify, merge, publish, distribute, sublicense,
| and/or sell copies of the Software, and to permit persons to whom the
| Software is furnished to do so, subject to the following conditions:
|
| The above copyright notice and this permission notice shall be included in
| all copies or substantial portions of the Software.
|
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
| IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
| FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
| AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
| LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
| FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
| DEALINGS IN THE SOFTWARE.
/===============================================================================================================*/

#ifndef _INIPARSER_H
#define _INIPARSER_H

/*---------------------------------------------------------------------------------------------------------------/
/ Includes
/---------------------------------------------------------------------------------------------------------------*/
#include <string>     // for std::string
#include <vector>     // for std::vector
#include <map>        // for std::map
#include <sstream>    // for std::stringstream
#include <algorithm>  // for std::transform
#include <functional> // for std::not1, std::ptr_fun
#include <cctype>     // for std::isspace()
#include <ctype.h>    // for tolower() and toupper()
#include <fstream>    // for std::fstream
#include <string.h>   // for strlen()
#include <iomanip>    // for std::setprecision
/*---------------------------------------------------------------------------------------------------------------/
/ Defines & Settings
/---------------------------------------------------------------------------------------------------------------*/

// Error codes
#define INI_ERR_INVALID_FILENAME  -1   // Can't open file for reading or writing
#define INI_ERR_PARSING_ERROR     -2   // File parse error

// INI file syntax can be changed here
// NOTE: When saving INI files first characters of provided arrays are used

/// Each of the characters in this string opens section
#define INI_SECTION_OPEN_CHARS   "[{"
/// Each of the characters in this string closes section
#define INI_SECTION_CLOSE_CHARS  "]}"
/// Each of the characters in this string starts comment
#define INI_COMMENT_CHARS         ";#"
/// Each of the characters in this string separates entry's name from entry's value
#define INI_NAME_VALUE_SEP_CHARS "=:"
/// Each of the characters in this string when found last in input line tells that next line
/// should be considered as current line's continuation
#define INI_MULTILINE_CHARS         "\\/"

// Delimiter of values in array
#define INI_ARRAY_DELIMITER         ','
// Symbol, opening the segment of array (INI_ARRAY_DELIMITER in segment is not considered as delimiter)
#define INI_ARRAY_SEGMENT_OPEN      '{'
// Symbol, closing the segment of array
#define INI_ARRAY_SEGMENT_CLOSE     '}'
// Escape character in INI file (INI_ARRAY_SEGMENT_OPEN and INI_ARRAY_SEGMENT_CLOSE if found inside
// elements of array and maps should be escaped for proper parsing)
#define INI_ESCAPE_CHARACTER        '\\'
// Symbol, separating key from value in map
#define INI_MAP_KEY_VAL_DELIMETER   ':'
// Delimiter, separating parent name from child name in section full name
#define INI_SUBSECTION_DELIMETER    '.'
// Delimiter to separate section name from value name when getting value directly from file
#define INI_SECTION_VALUE_DELIMETER ':'

// Inclusion line
// Please note, that inclusion command must appear in comment section (i.e. ;INI_INCLUDE_SEQ)
#define INI_INCLUDE_SEQ "#include "

// Path delimiter
#ifdef WIN32
#    define SYSTEM_PATH_DELIM '\\'
#else
#    define SYSTEM_PATH_DELIM '/'
#endif

/*---------------------------------------------------------------------------------------------------------------/
/ Auxiliaries
/---------------------------------------------------------------------------------------------------------------*/

// All parser-related stuff is in INI namespace
namespace INI
{
    /// String to lower case
    static inline void string_to_lower(std::string& str)
    {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    }

    /// String to upper case
    static inline void string_to_upper(std::string& str)
    {
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    }

    /// stream to be used for conversions
    class convstream: public std::stringstream
    {
    public:
        convstream() {*this << std::setprecision(17) << std::boolalpha;}
    };

    /// Convert anything (int, double etc) to string
    template<class T> std::string t_to_string(const T& i)
    {
        convstream ss;
        std::string s;
        ss << i;
        s = ss.str();
        return s;
    }
    /// Special case for string (to avoid overheat)
    template<> inline std::string t_to_string<std::string>(const std::string& i)
    { 
        return i; 
    }

    /// Convert string to anything (int, double, etc)
    template<class T> T string_to_t(const std::string& v)
    {
        convstream ss;
        T out;
        ss << v;
        ss >> out;
        return out;
    }
    /// Special case for string
    template<> inline std::string string_to_t<std::string>(const std::string& v)
    {
        return v;
    }
    /// Special case for boolean value
    /// std::boolalpha only covers 'true' or 'false', while we have more cases
    template<> inline bool string_to_t<bool>(const std::string& v)
    {
        if (!v.size())
            return false;
        if (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'Y' || v[0] == 'y')
            return true;
        return false;
    }

    // Calling isspace with signed chars in Visual Studio causes assert failure in DEBUG builds
    // This hack is approved by MS itself
#if defined (_MSC_VER)
    static inline int __in_isspace(int ch) {return std::isspace((unsigned char)ch);}
#else
#    define __in_isspace std::isspace
#endif

    // Trim string from start
    static inline std::string &ltrim(std::string &s) {
#if (__cplusplus >= 201103L)
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !__in_isspace(ch); }));
#else
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(__in_isspace))));
#endif
        return s;
    }

    // Trim string from end
    static inline std::string &rtrim(std::string &s) {
#if (__cplusplus >= 201103L)
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !__in_isspace(ch); }).base(), s.end());
#else
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(__in_isspace))).base(), s.end());
#endif
        return s;
    }

    // Trim string from both ends
    static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
    }

    /// Split strings (and trim result) based on the provided separator
    /// Strings in the vector would be trimmed as well
    static inline std::vector<std::string> split_string(const std::string& str, const std::string& sep)
    {
        std::vector<std::string> lst;
        if (str.empty())
            return lst;
        size_t cur_pos = 0;
        size_t prev_pos = 0;
        for (; ; cur_pos+=sep.size(),prev_pos = cur_pos)
        {
            cur_pos = str.find(sep,cur_pos);
            if (cur_pos == std::string::npos)
                break;
            std::string out = str.substr(prev_pos,cur_pos-prev_pos);
            trim(out);
            lst.push_back(out);
        }
        std::string out = str.substr(prev_pos);
        trim(out);
        lst.push_back(out);
        return lst;
    }

    /// Join array of strings into one with specified separator
    static inline std::string join_string(const std::vector<std::string>& array, const std::string& sep)
    {
        std::string ret;
        if (!array.size())
            return ret;
        for (size_t i = 0; i < array.size()-1; i++)
            ret += array.at(i) + sep;
        ret += array.at(array.size()-1);
        return ret;
    }

    /// Test whether provided char is in the charset
    static inline bool char_is_one_of(int ch, const char* charset)
    {
        for (const char* it = charset; *it != '\0'; ++it)
            if (ch == *it)
                return true;
        return false;
    }

    // Replace every occurrence of @param what to @param ret in @param str
    static inline std::string& str_replace(std::string& str, const std::string& what, const std::string& rep)
    {
        int diff = (int)(rep.size() - what.size() + 1);
        for (size_t pos = 0; ;pos += diff)
        {
            pos = str.find(what.c_str(),pos);
            if (pos == std::string::npos)
                break;
            str.replace(pos,what.size(),rep);
        }
        return str;
    }

    // normalize path (for relative path to be system-independent)
    static inline void normalize_path(std::string& path)
    {
#ifdef WIN32
        str_replace(path,"/","\\");
#else
        str_replace(path,"\\","/");
#endif
    }

    // get file path (excluding file name)
    static inline std::string file_path(const std::string& file_fullname)
    {
        size_t pos = file_fullname.rfind(SYSTEM_PATH_DELIM);
        if (pos == std::string::npos)
            return std::string();
        return file_fullname.substr(0,pos);
    }

    // get file name (excluding file path)
    static inline std::string file_name(const std::string& file_fullname)
    {
        size_t pos = file_fullname.rfind(SYSTEM_PATH_DELIM);
        if (pos == std::string::npos)
            return file_fullname;
        return file_fullname.substr(pos+1);
    }

    // check whether path is absolute
    static inline bool path_is_absolute(const std::string& path)
    {
#ifdef WIN32
        if (path.size() < 2)
            return false;
        // absolute filenames should start with drive letter (i.e. c:) 
        // or network path (\\)
        if (path[1] == ':' || (path[0] == '\\' && path[1] == '\\'))
            return true;
        return false;
#else
        // everything is simple under POSIX: absolute path should start with /
        if (path.size() < 1)
            return false;
        if (path[0] == '/')
            return true;
        return false;
#endif
    }

    // check whether path is relative
    static inline bool path_is_relative(const std::string& path) {return !path_is_absolute(path);}

    /*---------------------------------------------------------------------------------------------------------------/
    / INI-related classes
    /---------------------------------------------------------------------------------------------------------------*/

    // forward declarations
    class Value;
    class Array;
    class Map;
    class Section;
    class File;
    typedef std::vector<Section*> SectionVector;
    
    /**
      * Reference-counting helper class
      * This should be used as a member in reference-counting classes Value, Array and Map
    **/
    template<class T> class RefCountPtr
    {
    public:
        RefCountPtr():_ptr(NULL){}
        RefCountPtr(const RefCountPtr& cp):_ptr(cp._ptr) {Increment();}
        RefCountPtr(const T& val):_ptr(NULL){_ptr = new refcont(val);}
        virtual ~RefCountPtr() {Decrement();}
        RefCountPtr& operator= (const RefCountPtr& rt)
        {
            Decrement();
            _ptr = rt._ptr;
            Increment();
            return *this;
        }
        bool operator== (const RefCountPtr& val) const
        {
            if (_ptr == val._ptr)
                return true;
            if (!_ptr || !val._ptr)
                return false;
            return _ptr->val == val._ptr->val;
        }
        bool operator!= (const Value& val) const { return !(*this == val);}
        bool IsValid() const {return (_ptr != NULL);}
        T* DataPtr() {return (_ptr==NULL?NULL:&_ptr->val);}
        const T* DataPtr() const { return (_ptr==NULL?NULL:&_ptr->val);}
        T& Data() {return _ptr->val;}
        const T& Data() const {return _ptr->val;}
        T DataObj(const T& defval = T()) const {return (_ptr==NULL?defval:_ptr->val);}
        T* operator->() {return DataPtr();}
        const T* operator->() const {return DataPtr();}
        void Copy()
        {
            if (!_ptr)
                _ptr = new refcont();
            else if (_ptr->count > 1)
            {
                refcont* cp = new refcont(_ptr->val);
                Decrement();
                _ptr = cp;
            }
        }
    private:
        void Decrement()
        {
            if (!_ptr)
                return;
            _ptr->count--;
            if (!_ptr->count)
            {
                delete(_ptr);
                _ptr = NULL;
            }
        }
        void Increment()
        {
            if (_ptr)
                _ptr->count++;
        }
        struct refcont
        {
            refcont() :count(1) {}
            refcont(const T& pval) :val(pval), count(1) {}
            T val;
            size_t count;
        };
        refcont* _ptr;
    };

    /**
      * Value to be stored in INI-file
      * This is a simple reference-counting class, storing a pointer to original string
      * It has some functions for easy converting to\from other types
      * Value can contain array (in string representation) and be converted to\from it 
    **/
    class Value
    {
    public:
        Value(){}
        Value(const Value& cp):_val(cp._val){}
        template<class T> Value(const T& val){Set(val);}
        Value(const char* value){Set(value);}
        virtual ~Value(){}

        Value& operator= (const Value& rt) {_val = rt._val; return *this;}
        template<class T> Value& operator= (const T& value) { Set(value); return *this;}
        bool operator== (const Value& rgh) const { return _val == rgh._val;}
        bool operator!= (const Value& val) const { return !(*this == val);}
        bool operator< (const Value& rgh) const
        {
            if (!_val.IsValid())
                return true;
            if (!rgh._val.IsValid())
                return false;
            return (_val.Data() < rgh._val.Data());
        }

        /// Template function to convert value to any type
        template<class T> T Get() const { return string_to_t<T>(_val.DataObj());}
        void Set(const std::string& str) { _val = RefCountPtr<std::string>(str); }
        /// Template function to set value
        template<class T> void Set(const T& value) { Set(t_to_string(value)); }
        // const char* (as any pointer) is defaulting in template to int or bool, which is not what we want
        void Set(const char* value) { _val = RefCountPtr<std::string>(std::string(value)); }

        /// Converts Value to std::string
        std::string AsString() const { return _val.DataObj();}
        /// Converts Value to integer
        int AsInt() const {return Get<int>();}
        /// Converts Value to double
        double AsDouble() const {return Get<double>();}
        /// Converts Value to boolean
        bool AsBool() const {return Get<bool>();}
        /// Converts Value to Array
        Array AsArray() const;
        /// Converts Value to Map
        Map AsMap() const;
        /// Converts Value to specified type T
        template<class T> T AsT() const {return Get<T>();}

    private:
        RefCountPtr<std::string> _val;
    };

    /**
      * Array of Values
      * Reference-counting class
    **/
    class Array
    {
    public:
        Array(){}
        Array(const Array& cp) :_val(cp._val){}
        Array(const std::string& str, char sep = INI_ARRAY_DELIMITER, char seg_open = INI_ARRAY_SEGMENT_OPEN,
              char seg_close = INI_ARRAY_SEGMENT_CLOSE, char esc = INI_ESCAPE_CHARACTER)
        {
            FromString(str,sep,seg_open,seg_close,esc);
        }
        template<class T> Array(const std::vector<T>& vect){FromVector(vect);} 
        virtual ~Array() {}

        Array& operator= (const Array& rt) {_val = rt._val; return *this;}
        Array& operator<< (const Value & val) { return PushBack(val);}
        /// Returns reference to value on specified position @param pos
        /// Array is automatically widen (if needed) for that operation to always succeed
        Value& operator[] (size_t pos)
        {
            _val.Copy();
            if (pos >= _val->size())
                _val->insert(_val->end(),pos-_val->size()+1,Value());
            return _val.Data()[pos];
        }

        /// Gets value with specified position @param pos
        /// If @param pos is >= Size() returns @param def_val
        Value GetValue(size_t pos, const Value& def_val = Value()) const
        {
            if (!_val.IsValid())
                return def_val;
            if (pos >= _val->size())
                return def_val;
            return _val->at(pos);
        }
        /// Sets value @param value to specified position @param pos
        /// Array is automatically widen (if needed) to allow this operation 
        void SetValue(size_t pos, const Value& value)
        {
            _val.Copy();
            if (pos >= _val->size())
            {
                _val->insert(_val->end(),pos-_val->size(),Value());
                _val->push_back(value);
            }
            else
                _val->at(pos) = value;
        }
        /// Adds @param val to the end of the Array
        Array& PushBack(const Value& val) 
        {
            _val.Copy();
            _val->push_back(val); 
            return *this;
        }
        /// Returns array size
        size_t Size() const { if (!_val.IsValid()) return 0; return _val->size(); }
        
        /// Formats string with all values of this array represented as strings, separated by @param sep
        std::string ToString(char sep = INI_ARRAY_DELIMITER, char seg_open = INI_ARRAY_SEGMENT_OPEN,
                             char seg_close = INI_ARRAY_SEGMENT_CLOSE, char esc = INI_ESCAPE_CHARACTER) const
        {
            std::string ret;
            if (!_val.IsValid())
                return ret;
            for (size_t i = 0; i < _val->size(); i++)
            {
                std::string tmp = _val->at(i).AsString();
                std::string out;
                bool has_delim = false;
                for (size_t j = 0; j < tmp.size(); j++)
                {
                    if (tmp[j] == seg_open || tmp[j] == seg_close)
                        out.push_back(esc);
                    else if (tmp[j] == sep)
                        has_delim = true;
                    out.push_back(tmp[j]);
                }
                if (has_delim)
                    ret += seg_open + out + seg_close;
                else
                    ret += out;
                if (i != _val->size()-1)
                    ret += sep;
            }
            return ret;
        }
        /// Fills array with values from string @param str, separated by @param sep
        void FromString(const std::string& str, char sep = INI_ARRAY_DELIMITER, 
                        char seg_open  = INI_ARRAY_SEGMENT_OPEN,
                        char seg_close = INI_ARRAY_SEGMENT_CLOSE, char esc = INI_ESCAPE_CHARACTER)
        {
            _val.Copy();
            _val->clear();
            if (str.empty())
                return;
            std::string cur_str;
            int segm_cnt = 0;
            bool escaped = false;
            int preesc = 0;
            for (size_t i = 0; i <= str.size(); ++i)
            {
                if (escaped && i < str.size())
                {
                    cur_str.push_back(str[i]);
                    escaped = false;
                    if (str[i] == esc)
                        preesc = 2;
                }
                else if ((i == str.size()) || (str[i] == sep && !segm_cnt))
                {
                    trim(cur_str);
                    _val->push_back(cur_str);
                    cur_str.clear();
                }
                else if (str[i] == seg_open && !preesc)
                {
                    if (segm_cnt)
                        cur_str.push_back(str[i]);
                    segm_cnt++;
                }
                else if (str[i] == seg_close && !preesc)
                {
                    segm_cnt--;
                    if (segm_cnt < 0)
                        segm_cnt = 0;
                    if (segm_cnt)
                        cur_str.push_back(str[i]);
                }
                else if (str[i] == esc)
                    escaped = true;
                else
                    cur_str.push_back(str[i]);
                if (preesc)
                    preesc--;
            }
        }
        template<class T> std::vector<T> ToVector() const
        {
            std::vector<T> ret;
            if (!_val.IsValid())
                return ret;
            for (size_t i = 0; i < _val->size(); ++i)
                ret.push_back(_val->at(i).AsT<T>());
            return ret;
        }
        template<class T> void FromVector(const std::vector<T>& vect)
        {
            _val.Copy();
            _val->clear();
            for (size_t i = 0; i < vect.size(); ++i)
                _val->push_back(Value(vect.at(i)));
        }
        /// Converts Array to Value
        Value ToValue() const
        {
            return Value(ToString());
        }
        /// Gets Array from Value
        void FromValue(const Value& val)
        {
            FromString(val.AsString());
        }
    private:
        RefCountPtr<std::vector<Value> > _val;
    };

    template<> inline void Value::Set<Array> (const Array& value)
    {
        Set(value.ToString());
    }
    template<> inline Array Value::Get<Array>() const
    {
        if (!_val.IsValid())
            return Array();
        return Array(_val.Data());
    }
    inline Array Value::AsArray() const {return Get<Array>();}

    /**
      * Map of Values
      * Reference-counting class
    **/
    class Map
    {
    public:
        Map(){}
        Map(const Map& cp) :_val(cp._val){}
        Map(const std::string& str, char sep = INI_ARRAY_DELIMITER, char seg_open = INI_ARRAY_SEGMENT_OPEN,
              char seg_close = INI_ARRAY_SEGMENT_CLOSE, char kval = INI_MAP_KEY_VAL_DELIMETER,
              char esc = INI_ESCAPE_CHARACTER)
        {
            FromString(str,sep,seg_open,seg_close,kval,esc);
        }
        template<class T, class M> Map(const std::map<T,M>& mp){FromMap(mp);} 
        virtual ~Map(){}
        Map& operator= (const Map& rt) {_val = rt._val; return *this;}
        /// Returns reference to value of the specified key @param key
        /// Map is automatically inserts entry for this operation to always succeed
        Value& operator[] (const Value& key)
        {
            _val.Copy();
            return _val.Data()[key];
        }

        /// Gets value for specified key @param key
        /// If there is no specified key - returns @param def_val
        Value GetValue(const Value& key, const Value& def_val = Value()) const
        {
            if (!_val.IsValid())
                return def_val;
            std::map<Value,Value>::const_iterator it = _val->find(key);
            if (it == _val->end())
                return def_val;
            return it->second;
        }
        /// Sets value @param value to specified key @param key
        void SetValue(const Value& key, const Value& value)
        {
            _val.Copy();
            std::pair<std::map<Value,Value>::iterator,bool> res = 
                _val->insert(std::pair<Value,Value>(key,value));
            if (!res.second)
                res.first->second = value;
        }
        /// Returns map size
        size_t Size() const { if (!_val.IsValid()) return 0; return _val->size();}


        /// Formats string with all values of this map
        std::string ToString(char sep = INI_ARRAY_DELIMITER, char seg_open = INI_ARRAY_SEGMENT_OPEN,
                             char seg_close = INI_ARRAY_SEGMENT_CLOSE, char kval = INI_MAP_KEY_VAL_DELIMETER, 
                             char esc = INI_ESCAPE_CHARACTER) const
        {
            std::string ret;
            if (!_val.IsValid())
                return ret;
            for (std::map<Value,Value>::const_iterator it = _val->begin(); it != _val->end(); ++it)
            {
                if (it != _val->begin())
                    ret += sep;
                std::string key = it->first.AsString();
                std::string out_key;
                std::string val = it->second.AsString();
                std::string out_val;
                // key
                bool has_delim = false;
                for (size_t j = 0; j < key.size(); j++)
                {
                    if (key[j] == seg_open || key[j] == seg_close)
                        out_key.push_back(esc);
                    else if (key[j] == kval || key[j] == sep)
                        has_delim = true;
                    out_key.push_back(key[j]);
                }
                if (has_delim)
                    out_key = seg_open + out_key + seg_close;
                // value
                has_delim = false;
                for (size_t j = 0; j < val.size(); j++)
                {
                    if (val[j] == seg_open || val[j] == seg_close)
                        out_val.push_back(esc);
                    else if (val[j] == kval || val[j] == sep)
                        has_delim = true;
                    out_val.push_back(val[j]);
                }
                if (has_delim)
                    out_val = seg_open + out_val + seg_close;
                // pair
                ret += out_key + kval + out_val;
            }
            return ret;
        }
        /// Fills map with values from string @param str
        void FromString(const std::string& str, char sep = INI_ARRAY_DELIMITER, 
                        char seg_open  = INI_ARRAY_SEGMENT_OPEN,
                        char seg_close = INI_ARRAY_SEGMENT_CLOSE, char kval = INI_MAP_KEY_VAL_DELIMETER,
                        char esc = INI_ESCAPE_CHARACTER)
        {
            _val.Copy();
            _val->clear();
            if (str.empty())
                return;
            std::string cur_str;
            std::string cur_key;
            int segm_cnt = 0;
            bool escaped = false;
            int preesc = 0;
            for (size_t i = 0; i <= str.size(); ++i)
            {
                if (escaped && i < str.size())
                {
                    cur_str.push_back(str[i]);
                    escaped = false;
                    if (str[i] == esc)
                        preesc = 2;
                }
                else if ((i==str.size()) || (str[i] == sep && !segm_cnt))
                {
                    trim(cur_str);
                    std::pair<std::map<Value,Value>::iterator,bool> res = 
                        _val->insert(std::pair<Value,Value>(Value(cur_key),Value(cur_str)));
                    if (!res.second)
                        res.first->second = cur_str;
                    cur_str.clear();
                    cur_key.clear();
                }
                else if (str[i] == kval && !segm_cnt)
                {
                    trim(cur_str);
                    cur_key = cur_str;
                    cur_str.clear();
                }
                else if (str[i] == seg_open && !preesc)
                {
                    if (segm_cnt)
                        cur_str.push_back(str[i]);
                    segm_cnt++;
                }
                else if (str[i] == seg_close && !preesc)
                {
                    segm_cnt--;
                    if (segm_cnt < 0)
                        segm_cnt = 0;
                    if (segm_cnt)
                        cur_str.push_back(str[i]);
                }
                else if (str[i] == esc)
                    escaped = true;
                else
                    cur_str.push_back(str[i]);
                if (preesc)
                    preesc--;
            }
        }
        template<class T, class M> std::map<T,M> ToMap() const
        {
            std::map<T,M> ret;
            if (!_val.IsValid())
                return ret;
            for (std::map<Value,Value>::const_iterator it = _val->begin(); it != _val->end(); ++it)
                ret.insert(std::pair<T,M>(it->first.AsT<T>(),it->second.AsT<M>()));
            return ret;
        }
        template<class T, class M> void FromMap(const std::map<T,M>& mp)
        {
            _val.Copy();
            _val->clear();
            for (typename std::map<T,M>::const_iterator it = mp.begin(); it != mp.end(); ++it)
                _val->insert(std::pair<Value,Value>(Value(it->first),Value(it->second)));
        }
        /// Converts Array to Value
        Value ToValue() const
        {
            return Value(ToString());
        }
        /// Gets Array from Value
        void FromValue(const Value& val)
        {
            FromString(val.AsString());
        }
    private:
        RefCountPtr<std::map<Value,Value> > _val;
    };

    template<> inline void Value::Set<Map> (const Map& value)
    {
        Set(value.ToString());
    }
    template<> inline Map Value::Get<Map>() const
    {
        if (!_val.IsValid())
            return Map();
        return Map(_val.Data());
    }
    inline Map Value::AsMap() const {return Get<Map>();}

    /*---------------------------------------------------------------------------------------------------------------/
    / INI file creation and parsing
    /---------------------------------------------------------------------------------------------------------------*/

    /**
      * One section of the ini-file
      * This can be created by INIFile class only
    **/
    class Section
    {
        friend class File;
        typedef std::map<std::string, Value> EntryMap;
        typedef std::pair<std::string,Value> EntryPair;
        typedef std::map<std::string, std::string> CommentMap;
        typedef std::pair<std::string, std::string> CommentPair;
    /*-----------------------------------------------------------------------------------------------------------/
    / General functions & iterators
    /-----------------------------------------------------------------------------------------------------------*/
    public:
        typedef EntryMap::iterator values_iter;
        typedef EntryMap::const_iterator const_values_iter;
        values_iter ValuesBegin() {return _entries.begin();}
        const_values_iter ValuesBegin() const {return _entries.begin();}
        values_iter ValuesEnd() {return _entries.end();}
        const_values_iter ValuesEnd() const {return _entries.end();}
        size_t ValuesSize() const {return _entries.size();}

        /// Returns full section name (subsection name will contain '.', separating subsection part)
        const std::string& FullName() const { return _name;}
        /// Return section name (will be parsed to get individual section name)
        std::string Name() const
        {
            return _name.substr(_name.rfind(INI_SUBSECTION_DELIMETER)+1);
        }
        /// Get comment, associated with this section (written before the line with the section, or after it)
        std::string Comment() const {return _comment;}
        /// Return all keys in this section
        std::vector<std::string> GetSectionKeys() const
        {
            std::vector<std::string> keys;
            for (EntryMap::const_iterator it = _entries.begin(); it != _entries.end(); it++)
                keys.push_back(it->first);
            return keys;
        }
        /// Get Value
        Value GetValue(const std::string& key, const Value& def_value = Value()) const
        {
            EntryMap::const_iterator it = _entries.find(key);
            if (it == _entries.end())
                return def_value;
            return it->second;
        }
        /// Set Value
        void SetValue(const std::string& key, const Value& val, const std::string& comment = std::string())
        {
            _entries[key] = val;
            if (!comment.empty())
                _comments[key] = comment;
        }
        /// Set Value to array
        void SetArrayValue(const std::string& key, size_t pos, const Value& val)
        {
            Array ar = GetValue(key).AsArray();
            ar.SetValue(pos,val);
            SetValue(key,ar);
        }
        /// Remove value
        void RemoveValue(const std::string& key)
        {
            EntryMap::iterator it = _entries.find(key);
            if (it == _entries.end())
                return;
            _entries.erase(it);
        }
        /// Get comment, associated with provided value
        std::string GetComment(const std::string& key) const
        {
            CommentMap::const_iterator it = _comments.find(key);
            if (it == _comments.end())
                return std::string();
            return it->second;
        }
        /// Set comment for specified key
        void SetComment(const std::string& key, const std::string& comment)
        {
            _comments[key] = comment;
        }
        /// Save the section's contents to specified stream
        void Save(std::ostream& stream) const;
    /*-----------------------------------------------------------------------------------------------------------/
    / Parent & child parsing
    /-----------------------------------------------------------------------------------------------------------*/
    public:
        Section* FindParent() const;
        Section* GetParent();
        Section* FindSubSection(const std::string& name) const;
        Section* GetSubSection(const std::string& name);
        SectionVector FindSubSections() const;
    /*-----------------------------------------------------------------------------------------------------------/
    / Internal contents
    /-----------------------------------------------------------------------------------------------------------*/
    private:
        /// Only INI::File can create sections
        Section(File* file, const std::string& name, const std::string& comment = std::string()):
           _file(file),_name(name),_comment(comment){}
        /// Class should be copied only by File class to properly handle _file
        Section(const Section& cp):_file(cp._file),_name(cp._name),_comment(cp._comment),_entries(cp._entries),
            _comments(cp._comments){}
        File*       _file;                   // file, this section is associated with
        std::string _name;                 // name of the section
        std::string _comment;              // comment to the section
        EntryMap    _entries;              // all entries in the section
        CommentMap  _comments;             // all comments, associated with values in the section
    };

    /**
      * Main class of the parser
      * Provides way to load and save ini-files, as well as
      * setting specific values in them
    **/
    class File
    {
    public:
        /// Sections stores all values and comments inside them
        typedef std::map<std::string, Section*> SectionMap;
        typedef std::pair<std::string, Section*> SectionPair;
        typedef SectionMap::iterator          sections_iter;
        typedef SectionMap::const_iterator    const_sections_iter;
        /// Result of previous parse operation
        struct PResult
        {
            PResult():error_code(0),error_line_num(0){}
            void Set(int code, int line_num = 0, const std::string& line = std::string())
            {
                error_code = code;
                error_line_num = line_num;
                error_line = line;
            }
            void Invalidate() {error_code = 0; error_line_num = 0; error_line.clear();}
            std::string GetErrorDesc() const
            {
                if (!error_code)
                    return "No error";
                if (error_code == INI_ERR_INVALID_FILENAME)
                    return std::string("Failed to open file ") + file_name + "!";
                if (error_code == INI_ERR_PARSING_ERROR)
					return std::string("Parse error in file ") + file_name + " on line #"
                    + t_to_string(error_line) + ": \"" + error_line + "\""; 
                return "Unknown error!";
            }
            int error_code;         // code of the error. 0 if no error
            int error_line_num;     // number of line with the error
            std::string error_line; // line with error
            std::string file_name;  // name of the file with error (will be empty for stream)
            operator bool() const {return !error_code;}
        };
    public:
        File(){}
        File(const std::string& fname) { Load(fname);}
        File(const File& lf) { CopyFrom(lf); }
        File& operator= (const File& lf) { Unload(); CopyFrom(lf); return *this; }
        void CopyFrom(const File& lf)
        {
            for (SectionMap::const_iterator it = lf._sections.begin(); it != lf._sections.end(); it++)
            {
                Section* sect = new Section(*it->second);
                sect->_file = this;
                _sections.insert(SectionPair(it->first,sect));
            }
            _result = lf._result;
        }
        virtual ~File() {Unload();}
    /*---------------------------------------------------------------------------------------------------------------/
    / Section & values manipulations
    /---------------------------------------------------------------------------------------------------------------*/
    public:
        /// A way to iterate through all sections
        /// Section pointer can be accesed as SectionMap::iterator::second, section name - as ::first
        size_t SectionsSize() const {return _sections.size();}
        sections_iter SectionsBegin() {return _sections.begin();}
        const_sections_iter SectionsBegin() const {return _sections.begin();}
        sections_iter SectionsEnd() {return _sections.end();}
        const_sections_iter SectionsEnd() const {return _sections.end();}

        /// Get value from the file
        /// Use INI_SECTION_VALUE_DELIMETER to separate section name from value name
        Value GetValue(const std::string& name, const Value& def_val = Value())
        {
            size_t pos = name.rfind(INI_SECTION_VALUE_DELIMETER);
            std::string nm;
            if (pos != std::string::npos)
                nm = name.substr(0,pos);
            SectionMap::iterator it = _sections.find(nm);
            if (it == _sections.end())
                return def_val;
            return it->second->GetValue(name.substr(pos+1),def_val);
        }

        /// Set value to the file
        /// Use INI_SECTION_VALUE_DELIMETER to separate section name from value name
        void SetValue(const std::string& name, const Value& value, const std::string& comment = std::string())
        {
            size_t pos = name.rfind(INI_SECTION_VALUE_DELIMETER);
            std::string nm;
            if (pos != std::string::npos)
                nm = name.substr(0,pos);
            Section* sect = GetSection(nm);
            sect->SetValue(name.substr(pos+1),value,comment);
        }

        /// Set array Value to array
        void SetArrayValue(const std::string& key, size_t pos, const Value& val)
        {
            Array ar = GetValue(key).AsArray();
            ar.SetValue(pos,val);
            SetValue(key,ar);
        }

        /// Returns pointer to section with specified name
        /// If section does not exists - creates it
        Section* GetSection(const std::string& name)
        {
            SectionMap::iterator it = _sections.find(name);
            if (it != _sections.end())
                return it->second;
            Section* sc = new Section(this,name);
            _sections.insert(SectionPair(name,sc));
            return sc;
        }

        /// Find existing section by name
        /// @return pointer to existing section or NULL
        Section* FindSection(const std::string& name) const
        {
            SectionMap::const_iterator it = _sections.find(name);
            if (it == _sections.end())
                return NULL;
            return it->second;
        }

        /// Deletes section with specified name
        void DeleteSection(const std::string& name)
        {
            SectionMap::iterator it = _sections.find(name);
            if (it != _sections.end())
                _sections.erase(it);
        }

        /// Get subsection of specified section with specified name
        Section* FindSubSection(const Section* sect, const std::string& name) const
        {
            return FindSection(sect->FullName() + INI_SUBSECTION_DELIMETER + name);
        }

        /// If subsection does not exists, creates it
        Section* GetSubSection(Section* sect, const std::string& name)
        {
            return GetSection(sect->FullName() + INI_SUBSECTION_DELIMETER + name);
        }

        /// Get parent section of the specified section
        Section* FindParentSection(const Section* sect) const
        {
            size_t pos = sect->FullName().rfind(INI_SUBSECTION_DELIMETER);
            std::string nm;
            if (pos != std::string::npos)
                nm = sect->FullName().substr(0,pos);
            return FindSection(nm);
        }
        /// Get parent section (created if needed)
        Section* GetParentSection(const Section* sect)
        {
            size_t pos = sect->FullName().rfind(INI_SUBSECTION_DELIMETER);
            std::string nm;
            if (pos != std::string::npos)
                nm = sect->FullName().substr(0,pos);
            return GetSection(nm);
        }
        /// Find subsections of the specified section
        SectionVector FindSubSections(const Section* sect) const
        {
            SectionVector ret;
            if (sect->_file != this)
                return ret;
            for (SectionMap::const_iterator it = _sections.begin(); it != _sections.end(); ++it)
            {
                if (it->second == sect)
                    continue;
                if (it->first.find(sect->FullName() + INI_SUBSECTION_DELIMETER) != std::string::npos)
                    ret.push_back(it->second);
            }
            return ret;
        }
        // Get top-level sections (not child of any other sections)
        SectionVector GetTopLevelSections() const
        {
            SectionVector ret;
            for (SectionMap::const_iterator it = _sections.begin(); it != _sections.end(); ++it)
            {
                if (it->first.find(INI_SUBSECTION_DELIMETER) == std::string::npos)
                    ret.push_back(it->second);
            }
            return ret;
        }
    /*---------------------------------------------------------------------------------------------------------------/
    / Load & Save functions
    /---------------------------------------------------------------------------------------------------------------*/
    public:
        /// Load file from input stream, that must be properly opened and prepared for reading
        /// Set @param unload_prev to true for unloading any stuff currently in memory before loading 
        /// Set @param rpath to be used as relative path when searching for inclusions in files
        /// @return 1 if load succeeds, 0 if not
        /// in case load was not succesfull you can access extended information by calling ParseResult()
        int Load(std::istream& stream, bool unload_prev = false, const std::string& rpath = std::string())
        {
            if (unload_prev)
                DeleteSections(_sections);
            return ParseStream(stream,"",rpath,_sections);
        }
        /// Load ini from file in system
        /// Set @param unload_prev to false for not unloading any stuff currently in memory before loading 
        int Load(const std::string& fname, bool unload_prev = true)
        {
            _result.file_name = fname;
            normalize_path(_result.file_name);
            std::ifstream file(_result.file_name.c_str(), std::ios::in);
            if (!file.is_open())
            {
                _result.Set(INI_ERR_INVALID_FILENAME);
                return 0;
            }
            return Load(file,unload_prev,file_path(_result.file_name));
        }

        /// Save ini file to stream
        void Save(std::ostream& stream) const
        {
            SaveStream(stream,_sections); 
        }

        /// Save ini file to file
        int Save(const std::string& fname)
        {
            _result.file_name = fname;
            normalize_path(_result.file_name);
            std::ofstream file(_result.file_name.c_str(), std::ios::out);
            if (!file.is_open())
            {
                _result.Set(INI_ERR_INVALID_FILENAME);
                return 0;
            }
            SaveStream(file,_sections);
            return 1;
        }

        /// Save only one section to specifed stream
        void Save(std::ostream& stream, const Section* sect) const
        {
            SectionMap mp;
            mp.insert(SectionPair(sect->FullName(),const_cast<Section*>(sect)));
            SaveStream(stream,mp);
        }

        /// Adds comment line to provided stream
        /// For it to be not associated with anything you should add newline after it
        static void AddCommentToStream(std::ostream& stream, const std::string& str)
        {
            std::vector<std::string> ar = split_string(str,"\n");
            for (size_t i = 0; i < ar.size(); i++)
                stream << INI_COMMENT_CHARS[0] << ar.at(i) << std::endl;
        }

        /// Adds inclusion line to stream
        static void AddIncludeToStream(std::ostream& stream, const std::string& path)
        {
            stream << INI_COMMENT_CHARS[0] << INI_INCLUDE_SEQ << path << std::endl;
        }

        /// Unload memory
        void Unload()
        {
            DeleteSections(_sections);
        }
        /// Return last operation result
        const PResult& LastResult() {return _result;}
    /*---------------------------------------------------------------------------------------------------------------/
    / Parsing & Saving internals
    /---------------------------------------------------------------------------------------------------------------*/
    protected:
        enum LineType
        {
            LEKSYSINI_EMPTY,
            LEKSYSINI_SECTION,
            LEKSYSINI_ENTRY,
            LEKSYSINI_COMMENT,
            LEKSYSINI_ERROR
        };

        /**
          * Parse input line
          * Line must be concatenated (if needed) and trimmed before passing to this function
          * @param input_line - Line to parse
          * @param section - will contain section name in case SECTION return
          * @param key - will contain key in case ENTRY return
          * @param value - will contain value in case ENTRY return
          * @param comment - will contain comment in case of return != EMPTY and != ERROR
          * @return type of the parsed line
        **/
        LineType ParseLine(const std::string& input_line, std::string& section, 
                           std::string& key, std::string& value, std::string& comment) const
        {
            LineType ret = LEKSYSINI_EMPTY;
            size_t last_pos = input_line.npos;
            if (input_line.empty())
                return ret;
            // comment parsing
            if (char_is_one_of(input_line.at(0),INI_COMMENT_CHARS))
            {
                ret = LEKSYSINI_COMMENT;
                // Can't do it that way, cause of 'initialization of non-const reference of type from a temporary of type'
                // stupid GCC rule
                //comment = trim(input_line.substr(1));
                comment = input_line.substr(1);
                trim(comment);
                return ret;
            }
            // section parsing
            else if (char_is_one_of(input_line.at(0),INI_SECTION_OPEN_CHARS))
            {
                last_pos = input_line.find_first_of(INI_SECTION_CLOSE_CHARS);
                if (last_pos == input_line.npos)
                    return LEKSYSINI_ERROR;
                ret = LEKSYSINI_SECTION;
                section = input_line.substr(1,last_pos-1);
                trim(section);
                last_pos++;
            }
            // key-value pair parsing
            else
            {
                size_t pos = input_line.find_first_of(INI_NAME_VALUE_SEP_CHARS);
                // not section, not comment, not empty - error
                if (pos == input_line.npos /*|| pos == input_line.size()-1*/)
                    return LEKSYSINI_ERROR;
                ret = LEKSYSINI_ENTRY;
                key = input_line.substr(0,pos);
                trim(key);
                last_pos = input_line.find_first_of(INI_COMMENT_CHARS,pos+1);
                value = input_line.substr(pos+1,last_pos-pos-1);
                trim(value);
                if (last_pos != input_line.npos)
                    last_pos--;
            }
            // get associated comment
            last_pos = input_line.find_first_of(INI_COMMENT_CHARS,last_pos);
            if (last_pos != input_line.npos)
            {
                comment = input_line.substr(last_pos+1);
                trim(comment);
            }
            return ret;
        }

        /// Parse provided input stream to specified section map
        /// Comments separated from closest section or key-value pair by 1 or more empty strings
        /// would be ignored
        /// Default section will be created if needed
        int ParseStream(std::istream& stream, const std::string& def_section, 
                        const std::string& rpath, SectionMap& pmap)
        {
            Section* cur_sect = NULL;
            std::string pcomment;
            std::string prev_line;
            _result.Invalidate();

            // Find whether default section already exists in provided map
            // if not - it will be created later if needed
            SectionMap::iterator it = pmap.find(def_section);
            if (it != pmap.end())
                cur_sect = it->second;

            for (int lnc = 1; !stream.eof(); lnc++)
            {
                std::string line;
                std::getline(stream,line);
                trim(line);
                if (line.empty())
                {
                    pcomment.clear();
                    continue;
                }
                // Handle multiline strings
                if (char_is_one_of(line.at(line.size()-1),INI_MULTILINE_CHARS))
                {
                    prev_line = prev_line + line.substr(0,line.size()-1);
                    continue;
                }
                else if (!prev_line.empty())
                {
                    line = prev_line + line;
                    prev_line.clear();
                }
                std::string section_key, value, comment;
                LineType lt = ParseLine(line,section_key,section_key,value,comment);
                if (lt == LEKSYSINI_EMPTY)
                {
                    pcomment.clear();
                    continue;
                }
                else if (lt == LEKSYSINI_ERROR)
                {
                    _result.Set(INI_ERR_PARSING_ERROR,lnc,line);
                    return 0;
                }
                // Handle inclusion
                else if (lt == LEKSYSINI_COMMENT && comment.substr(0,strlen(INI_INCLUDE_SEQ)) == INI_INCLUDE_SEQ)
                {
                    std::string incname = comment.substr(strlen(INI_INCLUDE_SEQ));
                    trim(incname);
                    normalize_path(incname);
                    // try to open file
                    std::string fpath;
                    if (path_is_relative(incname) && !rpath.empty())
                        fpath = rpath + SYSTEM_PATH_DELIM + incname;
                    else
                        fpath = incname;
                    std::ifstream file(fpath.c_str(), std::ios::in);
                    std::string prevfn = _result.file_name;
                    _result.file_name = fpath;
                    if (!file.is_open())
                    {
                        _result.Set(INI_ERR_INVALID_FILENAME,lnc,line);
                        return 0;
                    }
                    std::string scname; 
                    if (cur_sect)
                        scname = cur_sect->FullName();
                    else
                        scname = def_section;
                    if (!ParseStream(file,scname,file_path(fpath),pmap))
                        return 0;
                    _result.file_name = prevfn;
                    continue;
                }
                // Add comment (it can be set with any string type, other than EMPTY and ERROR)
                pcomment += comment;
                // Add section (or modify comment of existing one if needed)
                if (lt == LEKSYSINI_SECTION)
                {
                    SectionMap::iterator it = pmap.find(section_key);
                    if (it == pmap.end())
                    {
                        cur_sect = new Section(this,section_key,pcomment);
                        pmap.insert(SectionPair(section_key, cur_sect));
                    }
                    else
                    {
                        cur_sect = it->second;
                        if (!pcomment.empty())
                        {
                            if (!it->second->_comment.empty())
                                it->second->_comment += stream.widen('\n') + pcomment;
                            else
                                it->second->_comment = pcomment;
                        }
                    }
                    pcomment.clear();
                }
                else if (lt == LEKSYSINI_ENTRY)
                {
                    // Try to create default section if it is not already in the array
                    if (!cur_sect)
                    {
                        cur_sect = new Section(this,def_section);
                        pmap.insert(SectionPair(def_section, cur_sect));
                    }
                    cur_sect->SetValue(section_key,value,pcomment);
                    pcomment.clear();
                }
            }
            // Clears eof flag for future usage of stream
            stream.clear();
            return 1;
        }
        /// Save provided section map to stream
        void SaveStream(std::ostream& stream, const SectionMap& pmap) const
        {
            for (SectionMap::const_iterator it = pmap.begin(); it != pmap.end(); ++it)
            {
                if (it->second->ValuesSize() == 0)
                    continue;
                if (!it->second->Comment().empty())
                    AddCommentToStream(stream,it->second->Comment());
                if (!it->first.empty())
                    stream << INI_SECTION_OPEN_CHARS[0] << it->first << INI_SECTION_CLOSE_CHARS[0] << std::endl;
                for (Section::values_iter vit = it->second->ValuesBegin(); vit != it->second->ValuesEnd(); vit++)
                {
                    stream << vit->first << " " << INI_NAME_VALUE_SEP_CHARS[0] << " " << vit->second.AsString();
                    std::string cmn = it->second->GetComment(vit->first);
                    if (!cmn.empty())
                        stream << " " << INI_COMMENT_CHARS[0] << cmn;
                    stream << std::endl;
                }
                stream << std::endl;
            }
        }
    private:
        void DeleteSections(SectionMap& mp)
        {
            for (SectionMap::iterator it = mp.begin(); it != mp.end(); ++it)
                delete(it->second);
            mp.clear();
        }
        // All sections (including subsections) in one map
        SectionMap        _sections;
        PResult       _result;
    };

    /*-----------------------------------------------------------------------------------------------------------/
    / Some functions left unimplemented
    /-----------------------------------------------------------------------------------------------------------*/
    inline Section* Section::GetParent() {return _file->GetParentSection(this);}
    inline Section* Section::FindParent() const {return _file->FindParentSection(this);}
    inline Section* Section::GetSubSection(const std::string& name) {return _file->GetSubSection(this,name);}
    inline Section* Section::FindSubSection(const std::string& name) const {return _file->FindSubSection(this,name);}
    inline SectionVector Section::FindSubSections() const {return _file->FindSubSections(this);}
    inline void Section::Save(std::ostream& stream) const {return _file->Save(stream,this);}
}
/*---------------------------------------------------------------------------------------------------------------/
/ Stream operators
/---------------------------------------------------------------------------------------------------------------*/
static inline std::ostream& operator<< (std::ostream& stream, const INI::File& file)
{
    file.Save(stream);
    return stream;
}
static inline std::ostream& operator<< (std::ostream& stream, const INI::Section* sect)
{
    sect->Save(stream);
    return stream;
}
static inline std::ostream& operator<< (std::ostream& stream, const INI::Array& ar)
{
    stream << ar.ToValue().AsString();
    return stream;
}
static inline std::istream& operator>> (std::istream& stream, INI::File& file)
{
    file.Load(stream,false);
    return stream;
}

/*---------------------------------------------------------------------------------------------------------------/
/ Little tidying up
/---------------------------------------------------------------------------------------------------------------*/
#undef INI_SECTION_OPEN_CHARS
#undef INI_SECTION_CLOSE_CHARS
#undef INI_COMMENT_CHARS
#undef INI_NAME_VALUE_SEP_CHARS
#undef INI_MULTILINE_CHARS
#undef INI_ARRAY_DELIMITER
#undef INI_ARRAY_SEGMENT_OPEN
#undef INI_ARRAY_SEGMENT_CLOSE
#undef INI_ESCAPE_CHARACTER
#undef INI_MAP_KEY_VAL_DELIMETER
#undef INI_SUBSECTION_DELIMETER
#undef INI_SECTION_VALUE_DELIMETER
#undef INI_INCLUDE_SEQ
#undef SYSTEM_PATH_DELIM
// Note: error definitions are left

#endif
