package main

import (
	"bufio"
	"bytes"
	"flag"
	"fmt"
	"io"
	"os"
	"path"
	"strings"
	"unicode"
)

var PROGRAM_NAME string = path.Base(os.Args[0])

func Error(exitStatus int, err error, format string, args ...interface{}) {
	buf := bytes.Buffer{}
	buf.WriteString(fmt.Sprintf("%s: ", PROGRAM_NAME))
	buf.WriteString(fmt.Sprintf(format, args...))
	if err != nil {
		buf.WriteString(fmt.Sprintf(": %s", err))
	}
	buf.WriteByte('\n')

	os.Stdout.Sync()
	fmt.Fprint(os.Stderr, buf.String())
	os.Stderr.Sync()

	if exitStatus != 0 {
		os.Exit(exitStatus)
	}
}

var flag_help bool
var flag_no_header bool

func init() {
	// flag.BoolVar(&flag_help, "help", false, "display this help message and exit")
	flag.BoolVar(&flag_no_header, "no-header", false, "do not display the header")
	flag.BoolVar(&flag_no_header, "n", false, "do not display the header")
}

func help_and_exit() {
	msg := `Dump Unicode Rune and category
Usage: %s [OPTION...] [ARGUMENT...]

  -help             show this help message and exit
  -n, -no-header    do not display the header

If no argument specified, this program will read from Standard input.

Description of Output Flags

    D -- Digit, a decimal digit
    N -- a number, category N
    L -- a letter, category L
    G -- a Graphic; includes letters, marks, numbers, punctuation, symbols, and spaces.
    M -- a mark character, category M
    C -- a control character, a subset of category C.
    p -- a puncuation character, category P
    P -- a printable (by Go)
    s -- a space character
    S -- a symbolic character
    t -- a title case letter
    l -- a lower case letter
    u -- an upper case letter

Note that some of flags are not necessarily mean the unicode category.  
See https://golang.org/pkg/unicode/ for more.

`
	fmt.Printf(msg, PROGRAM_NAME)
	os.Exit(0)
}

func DisplayHeader() {
	header := `[D]igit  [N]umber  [L]etter  [G]raphic  [M]ark   [C]ontrol
[p]unct  [P]rint   [S]ymbol  [t]itle    [l]ower  [u]pper
--`
	fmt.Println(header)
}

func main() {
	flag.Parse()

	if flag_help {
		help_and_exit()
	}

	var reader *bufio.Reader

	if len(flag.Args()) == 0 {
		reader = bufio.NewReader(os.Stdin)
	} else {
		reader = bufio.NewReader(strings.NewReader(strings.Join(flag.Args(), " ")))
	}

	if !flag_no_header {
		DisplayHeader()
	}

	for {
		r, sz, err := reader.ReadRune()
		if err != nil {
			if err == io.EOF {
				break
			} else {
				Error(1, err, "reading a rune failed")
			}
		}

		fmt.Printf("%s\n", dumpRune(r, sz))
	}
}

func dumpRune(r rune, size int) string {
	buf := bytes.Buffer{}

	buf.WriteString(fmt.Sprintf("%08x [%d] ", r, size))

	type pair struct {
		symbol    rune
		predicate func(rune) bool
	}

	predicates := []pair{
		{'D', unicode.IsDigit},
		{'N', unicode.IsNumber},
		{'L', unicode.IsLetter},
		{'G', unicode.IsGraphic},
		{'M', unicode.IsMark},
		{'C', unicode.IsControl},
		{'p', unicode.IsPunct},
		{'P', unicode.IsPrint},
		{'s', unicode.IsSpace},
		{'S', unicode.IsSymbol},
		{'t', unicode.IsTitle},
		{'l', unicode.IsLower},
		{'u', unicode.IsUpper},
	}

	for _, p := range predicates {
		if p.predicate(r) {
			buf.WriteString(string(p.symbol))
		} else {
			buf.WriteString("_")
		}
	}

	if unicode.IsPrint(r) {
		buf.WriteString(fmt.Sprintf(" '%c'", r))
	}

	return buf.String()
}
