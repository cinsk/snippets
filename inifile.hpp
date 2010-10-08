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
#ifndef INIFILE_HPP__
#define INIFILE_HPP__

// $Id$

#ifndef __cplusplus
#error This is a C++ header file
#endif

#include <locale>
#include <string>
#include <iosfwd>
#include <fstream>
#include <string>
#include <map>

//
// Since there is no explicit specification for INI file, here is
// INI file description that this module understands.
//
// The basic element contained in an INI file is the *parameter*.
// A parameter has a form "NAME = VALUE".
//
// NAME can contain whitespace character in the middle of NAME.
// Neither a whitespace in the beginning or in the ending is allowed.
//
// VALUE can be a quoted string (using ' or "), or unquoted string.
//
// If VALUE is unquoted, it cannot contain a newline character neither
// in the beginning nor in the ending.
//
// For a quoted VALUE, escape sequence like C string is allowed.  To
// use a quotation character (' or ") in the string, you need to
// escape it.  You can use \xhh, \Xhh, \ooo in the escape sequence
// where 'h' represents an hexadecimal digit, and 'o' represents an
// octal digit.  The number of 'h' and 'o' is very important.
//
// *Parameters* may be grouped into a *section*.  A section name
// appears on a line by itself, in a square bracket.  (e.g. [SECTION])
//
// SECTION can contain whitespace character in the middle of it.
// There is no explicit "end of section" delimiter; sections ends at
// the next section declaration, or the end of file.  Nesting is not
// allowed.  All sections that have the same section name will be
// merged like C++ namespaces.
//
// When *parameter* appears with no SECTION declaration is in effect,
// the *paramter* belongs to the default SECTION.  The default section
// has an empty name.
//
// Blank lines or lines that have only whitespaces are ignored.
//
// '#' or ';' indicate the start of a comment.  Comments continue to
// the end of the line.  Note that comments can be placed at the same
// as *parameter*.  If the parameter's VALUE is unquoted, VALUE cannot
// have a '#' or ';' in it.  To use '#' or ';' in the VALUE, VALUE
// must be quoted.
//

class inifile {
public:
  //typedef std::multimap<std::string, std::string> section_type;
  typedef std::map<std::string, std::string> section_type;
  typedef std::map<std::string, section_type *> config_type;
  typedef config_type::value_type value_type;
  typedef config_type::size_type size_type;

  typedef config_type::iterator iterator;
  typedef config_type::reverse_iterator reverse_iterator;
  typedef config_type::const_reverse_iterator const_reverse_iterator;
  typedef config_type::const_iterator const_iterator;

  inifile();
  ~inifile();

  // Load the INI file.
  //
  // Returns true if parsing was successful, otherwise returns false.
  bool load(const char *pathname);

  // Return the SECTION_NAME section if exists.
  const section_type *section(const std::string &section_name = "") const;
  section_type *section(const std::string &section_name = "");

  // TODO: I'm not sure whether it is good idea to mimic a STL container.
  // TODO: Perhaps exposing config_ is better?
  iterator begin()                      { return config_.begin(); }
  const_iterator begin() const          { return config_.begin(); }
  iterator end()                        { return config_.end(); }
  const_iterator end() const            { return config_.end(); }
  const_reverse_iterator rbegin() const { return config_.rbegin(); }
  reverse_iterator rbegin()             { return config_.rbegin(); }
  reverse_iterator rend()               { return config_.rend(); }
  const_reverse_iterator rend() const   { return config_.rend(); }

  // Remove all sections.
  void clear();

  // Returns the number of the sections
  size_type size() const        { return config_.size(); }

private:
  typedef std::ifstream::char_type char_type;
  typedef std::ifstream::int_type int_type;
  typedef std::ifstream::traits_type traits_type;

  section_type *create_section(const std::string &name = "");
  bool register_parameter(section_type *sect,
                          const std::string &name, const std::string &value);

  void incr_lineno(int amount = 1) { lineno_ += amount; }
  void incr_lineno(std::istream &is, const std::string &s);

  void eat_spaces(std::istream &is, char_type lookahead = 0);
  void eat_comment(std::istream &is, char_type lookahead = 0);
  bool get_section_name(std::string &name,
                       std::istream &is, char_type lookahead = 0);

  bool get_param_name(std::string &name,
                      std::istream &is, char_type lookahead = 0);
  bool get_param_value(std::string &name,
                       std::istream &is, char_type lookahead = 0);

  bool get_esc_hex(int_type &value, std::istream &is);
  bool get_esc_oct(int_type &value, std::istream &is);


  const std::string &filename() { return filename_; }
  long lineno() { return lineno_; }
  std::ostream *estream() { return es_; }

  const std::locale locale_;

  std::string filename_;
  long lineno_;
  std::ostream *es_;
  config_type config_;
};

#endif  // INIFILE_HPP__
