/*
 * inifile: INI file reader / writer
 * Copyright (C) 2010  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// TODO: saving configuration to the file.
// TODO: use exception for errors (with better error messages)
// TODO: variable substitution like bash(1)
// TODO: multiple values for parameters
// TODO: line number handling

#include <iostream>
#include <locale>
#include <limits>
#include <sstream>

#include "inifile.hpp"

class inierror_voidify {
public:
  inierror_voidify() {}
  void operator&(std::ostream &) {}
};

#define ERR(inifile, type)                                      \
  false ? (void) 0 :                                            \
  inierror_voidify() & inierror(type, (inifile)->estream(),     \
                                (inifile)->filename(),          \
                                (inifile)->lineno()).stream()

enum inierror_type {
  ie_none,
  ie_warning,
  ie_error,
};

class inierror {
public:
  inierror(inierror_type type, std::ostream *os,
           const std::string &filename, long lineno = 0)
    : type_(type), filename_(filename), lineno_(lineno), os_(os), ss_() {};
  ~inierror() {
    if (os_) {
      if (!filename_.empty())
        (*os_) << filename_ << ":";
      if (lineno_ > 0)
        (*os_) << lineno_ << ": ";
      else
        (*os_) << " ";

      switch (type_) {
      case ie_none:
        break;
      case ie_warning:
        (*os_) << "warning: ";
      case ie_error:
        (*os_) << "error: ";
      }
      (*os_) << ss_.str() << std::endl;
    }
  }

  std::ostream &stream() { return ss_; }

private:
  inierror_type type_;
  std::string filename_;
  long lineno_;

  std::ostream *os_;
  std::ostringstream ss_;
};


static void
rstrip(std::string &s, const std::locale &loc = std::locale())
{
  std::string::iterator trimhere = s.begin();
  for (std::string::iterator i = s.begin(); i != s.end(); ++i) {
    if (!std::isspace(*i, loc)) {
      trimhere = i;
      ++trimhere;
    }
  }
  if (trimhere != s.end())
    s.erase(trimhere, s.end());
}


static void
lstrip(std::string &s, const std::locale &loc = std::locale())
{
  std::string::iterator i = s.begin();
  for (; i != s.end(); ++i) {
    if (!std::isspace(*i, loc))
      break;
  }
  s.erase(s.begin(), i);
}


static void
strip(std::string &s, const std::locale &loc = std::locale())
{
  rstrip(s, loc);
  lstrip(s, loc);
}

// If I made the constructor in following way:
//
// inifile::inifile() : locale_(std::locale) ... { }
//
// it would cause std::bad_cast exception.  Don't know why...
//
inifile::inifile()
  : locale_(std::locale()), lineno_(0), es_(&std::cerr)
{
}


inifile::~inifile()
{
  clear();
}


inifile::section_type *
inifile::section(const std::string &section_name)
{
  config_type::iterator i = config_.find(section_name);
  if (i == config_.end())
    return 0;
  else
    return i->second;
}


const inifile::section_type *
inifile::section(const std::string &section_name) const
{
  config_type::const_iterator i = config_.find(section_name);
  if (i == config_.end())
    return 0;
  else
    return i->second;
}


void
inifile::clear(void)
{
  for (config_type::iterator i = config_.begin(); i != config_.end(); ++i)
    delete i->second;
}


void
inifile::incr_lineno(std::istream &is, const std::string &s)
{
  for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
    if (*i == is.widen('\n'))
      lineno_++;
  }
}


void
inifile::eat_spaces(std::istream &is, char_type lookahead)
{
  char_type ch;

  if (lookahead)
    is.unget();

  while ((is >> ch)) {
    if (ch == is.widen('\n'))
      incr_lineno();
    else if (!std::isspace(ch, locale_)) {
      is.unget();
      break;
    }
  }
}


void
inifile::eat_comment(std::istream &is, char_type lookahead)
{
  is.ignore(std::numeric_limits<std::streamsize>::max(), is.widen('\n'));
  incr_lineno();
}


bool
inifile::get_section_name(std::string &name,
                          std::istream &is, char_type lookahead)
{
  eat_spaces(is, lookahead);
  getline(is, name);

  incr_lineno(is, name);

  std::string::size_type i = name.find_first_of(is.widen(']'));
  if (i == std::string::npos) {
    ERR(this, ie_error) << "missing ']' in the section declaration";

    return false;
  }
  else
    name.erase(i);

  rstrip(name);

  return true;
}


bool
inifile::get_esc_hex(int_type &value, std::istream &is)
{
  int_type hex[2];

  for (unsigned i = 0; i < sizeof(hex) / sizeof(int_type); ++i) {
    if ((hex[i] = is.get()) == traits_type::eof()) {
      ERR(this, ie_error) << "prematured escape sequence";
      return false;
    }
    else {
      if (hex[i] >= is.widen('0') && hex[i] <= is.widen('9'))
        hex[i] -= is.widen('0');
      else if (hex[i] >= is.widen('a') && hex[i] <= is.widen('f'))
        hex[i] -= is.widen('a');
      else if (hex[i] >= is.widen('A') && hex[i] <= is.widen('F'))
        hex[i] -= is.widen('A');
      else {
        ERR(this, ie_error) << "invalid hexadecimal escape sequence";
        return false;
      }
    }
  }

  value = 0;
  for (unsigned i = 0; i < sizeof(hex) / sizeof(int_type); ++i, value <<= 4)
    value += static_cast<unsigned>(hex[i]);

  return true;
}


bool
inifile::get_esc_oct(int_type &value, std::istream &is)
{
  int_type oct[3];

  for (unsigned i = 0; i < sizeof(oct) / sizeof(int_type); ++i)
    if ((oct[i] = is.get()) == traits_type::eof()) {
      ERR(this, ie_error) << "prematured escape sequence";
      return false;
    }
    else {
      if (oct[i] >= is.widen('0') && oct[i] <= is.widen('7'))
        oct[i] -= is.widen('0');
      else {
        ERR(this, ie_error) << "invalid octal escape sequence";
        return false;
      }
    }

  value = 0;
  for (unsigned i = 0; i < sizeof(oct) / sizeof(int_type); ++i, value <<= 3)
    value += static_cast<unsigned>(oct[i]);
  return true;
}


bool
inifile::get_param_value(std::string &name,
                         std::istream &is, char_type lookahead)
{
  char_type ch;

  if (lookahead)
    is.unget();

  eat_spaces(is);

  name.clear();

  if (!(is >> ch))
    return false;

  if (ch == is.widen('\"') || ch == is.widen('\'')) {
    // get_quoted_string
    char_type quote = ch;
    int_type ch, nextch;

    while ((ch = is.get()) != traits_type::eof()) {
      if (ch == is.widen('\\')) {
        // parse an escape sequence
        nextch = is.get();
        if (nextch == traits_type::eof()) {
          ERR(this, ie_error) << "prematured escape sequence";
          return false;
        }
        if (nextch == is.widen('a'))
          name.push_back(is.widen('\a'));
        else if (nextch == is.widen('b'))
          name.push_back(is.widen('\b'));
        else if (nextch == is.widen('f'))
          name.push_back(is.widen('\f'));
        else if (nextch == is.widen('n'))
          name.push_back(is.widen('\n'));
        else if (nextch == is.widen('r'))
          name.push_back(is.widen('\r'));
        else if (nextch == is.widen('t'))
          name.push_back(is.widen('\t'));
        else if (nextch == is.widen('v'))
          name.push_back(is.widen('\v'));
        else if (nextch == is.widen('\''))
          name.push_back(is.widen('\''));
        else if (nextch == is.widen('\"'))
          name.push_back(is.widen('\"'));
        else if (nextch == is.widen('x') || nextch == is.widen('X')) {
          // parse hexadecimal escape sequence
          int_type value;
          if (!get_esc_hex(value, is))
            return false;
          name.push_back(is.widen(value));
        }
        else if (nextch >= is.widen('0') && nextch <= is.widen('7')) {
          // parse octal escape sequence
          int_type value;
          if (!get_esc_oct(value, is))
            return false;
          name.push_back(is.widen(value));
        }
        else {
          // unknown escape sequence
        }
      }
      else if ((ch != quote && ch == '\'') || (ch != quote && ch == '\"')) {
        // a quote character that is not the starting quote character
        name.push_back(ch);
      }
      else if (ch == quote) {
        // end of the quoted string
        break;
      }
      else {
        // Even if we found a newline character inside of quoted
        // string, we accept it here.
        if (ch == is.widen('\n'))
          incr_lineno();

        name.push_back(ch);
      }
    } // end of while

    if (is) {
      // Once we got the quote string value, it is still possible that
      // there is some characters before the newline character.  If
      // remaining characters are all whitespaces, that is fine.
      // However, if any of the remaining characters is non-whitespace,
      // that is a garbage.
      std::string garbage;
      getline(is, garbage);
      incr_lineno();
      strip(garbage);
      // TODO: handle comment
      if (!garbage.empty() &&
          garbage[0] != is.widen(';') && garbage[0] != is.widen('#'))
        ERR(this, ie_warning) << "ignoring remainig characters '"
                              << garbage << "'";
      return true;
    }
    else {
      ERR(this, ie_error) << "unexpected EOF encountered";
      return false;
    }
  }
  else {
    // We accept empty value.
    is.unget();
    getline(is, name);
    incr_lineno();

    {
      // Remove any comment
      std::string::size_type i = name.find_first_of(is.widen(';'));
      if (i != std::string::npos)
        name.erase(i);
      i = name.find_first_of(is.widen('#'));
      if (i != std::string::npos)
        name.erase(i);
    }

    rstrip(name);
    return true;
  }

}


bool
inifile::get_param_name(std::string &name,
                        std::istream &is, char_type lookahead)
{
  if (lookahead)
    is.unget();

  int old_lineno = lineno_;
  getline(is, name, is.widen('='));
  incr_lineno(is, name);

  if (old_lineno != lineno_) {
    ERR(this, ie_error) << "parameter name contains newline character(s)";
    return false;
  }
  rstrip(name);
  return true;
}


inifile::section_type *
inifile::create_section(const std::string &name)
{
  config_type::iterator i = config_.find(name);
  if (i != config_.end())
    return i->second;

  section_type *p = new section_type;
  if (!p)
    return 0;

  config_type::value_type value(name, p);
  config_.insert(value);
  return p;
}


bool
inifile::register_parameter(section_type *sect,
                            const std::string &name,
                            const std::string &value)
{
  if (!sect)
    sect = create_section();

  // Currently, NAME can have only one VALUE.

  section_type::iterator i = sect->find(name);

  if (i != sect->end())
    sect->erase(i);

  section_type::value_type v(name, value);
  sect->insert(v);

  return true;
}


bool
inifile::load(const char *pathname)
{
  std::ifstream is(pathname, std::ios_base::in);
  section_type *current_section = NULL;

  filename_ = pathname;
  lineno_ = 0;

  if (!is.is_open()) {
    ERR(this, ie_error) << "cannot open '" << pathname << "'";
    return false;
  }
  incr_lineno();

  is >> std::noskipws;
  while (is) {
    eat_spaces(is);

    std::ifstream::char_type ch;

    if (!(is >> ch))
      break;

    if (ch == is.widen('#') || ch == is.widen(';')) {
      eat_comment(is, ch);
      continue;
    }
    else if (ch == is.widen('[')) {
      std::string name;
      if (!get_section_name(name, is, ch))
        return false;
#if 0
      std::cout << "Section: [" << name << "]" << std::endl;
#endif  // 0

      current_section = create_section(name);
    }
    else {
      std::string name, value;
      if (!get_param_name(name, is, ch))
        return false;

      if (!get_param_value(value, is))
        return false;
#if 0
      std::cout << "[" << name << "] = ["
                << value << "]" << std::endl;
#endif  // 0
      register_parameter(current_section, name, value);
    }
  }
  return true;
}


#ifdef TEST_INIFILE
int
main(int argc, char *argv[])
{
  inifile conf;

  for (int i = 1; i < argc; ++i) {
    if (!conf.load(argv[i]))
      continue;

    for (inifile::iterator i = conf.begin(); i != conf.end(); ++i) {
      std::cout << "[" << i->first << "]" << std::endl;

      for (inifile::section_type::iterator j = i->second->begin();
           j != i->second->end(); ++j) {
        std::cout << "     [" << j->first << "] = ["
                  << j->second << "]" << std::endl;
      }
    }
  }

  return 0;
}
#endif  // TEST_INIFILE
