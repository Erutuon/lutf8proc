# lutf8proc

`lutf8proc` is a Lua 5.3 binding of some of the functions from the [utf8proc](https://github.com/JuliaStrings/utf8proc) library. I created it for fun so it isn't well tested and its behavior isn't set in stone. I mainly use the normalization functions. See the testcases for examples.

## Functions

### Functions for retrieving information on a character
These take either a string or a number (which is interpreted as a code point). They currently allow any length of string, but only return information on the first character.

**`utf8proc.cat`**

Returns the General Category of a code point.
    utf8proc.cat "a", utf8proc.cat(0x61) --> "Ll", "Ll"

**`utf8proc.catdesc`**

Returns a long form of the General Category of a code point.
    utf8proc.catdesc "a", utf8proc.catdesc(0x61) --> "Letter, lowercase", "Letter, lowercase"

**`utf8proc.valid(code_point)`**

Returns true if a codepoint is valid, false if not. Checks that it is below 0x10FFFF and not a surrogate codepoint.

### Functions for transforming UTF-8 strings

These take and return a string.

**`utf8proc.comp, utf8proc.decomp, utf8proc.ccomp, utf8proc.cdecomp`**

Returns a composed (NFC), decomposed (NFD), compatibility composed (NFKC), or compatibility decomposed (NFKD) version of a string.

    utf8proc.decomp "á" --> "á" (U+00E1 -> U+0061 U+0301)
    utf8proc.comp "á" --> "á" (U+0061 U+0301 -> U+00E1)

**`utf8proc.normalize(str, normalization)`**

Returns a [normalized](https://en.wikipedia.org/wiki/Unicode_normalization) form of a string. Generalized version of the functions above. `normalization` is case-insensitive. For instance, `utf8proc.normalize("á", "nfd")` and `utf8proc.normalize("á", "NFD")` are both equivalent to `utf8proc.decomp "á"`.

**`utf8proc.lower, utf8proc.upper, utf8proc.title`**

Returns a lowercase, uppercase, or titlecase version of a string. There are only a few code points for which titlecase is different from uppercase.

**`utf8proc.map(str, options)` (bit-flag options) or `utf8proc.map(str, option1[, option2, ...])` (string options)**

Transform a string in a custom way. Options are either numbers (bit flags) or strings depending on how you compile the library. String options are case-insensitive.

Bit flags:
```
local options = utf8proc.options()
utf8proc.map("αὐτός", options.stripmark | options.decompose | options.casefold) --> "αυτοσ"
```
Strings:
```
utf8proc.map("αὐτός", "stripmark", "decompose", "casefold") --> "αυτοσ"
utf8proc.map("αὐτός", "STRIPMARK", "DECOMPOSE", "CASEFOLD") --> "αυτοσ"
```

**`utf8proc.map_custom(str, options, function)` (bit-flag options) or `utf8proc.map_custom(str, function[, option1[, option2, ...]])` (string options)**

Apply a function to each codepoint in a string.

    utf8proc.map_custom("abc", function (cp) return cp - 0x20 end) --> "ABC"

**`utf8proc.interpret_options(options)` (bit-flag options)**

Returns the list of string names for an integer. Throws an error if the integer contains bits that do not correspond to a flag.
