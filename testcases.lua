-- These testcases can only be parsed by Lua 5.3 and greater because of the bitwise operators.

utf8proc = require 'lutf8proc'
local options = utf8proc.options

local f = io.open('tests.txt', 'wb')
io.output(f)

local old_print = print
local print = function (...)
	for i = 1, select('#', ...) do
		if i ~= 1 then
			io.write '\t'
		end
		io.write((select(i, ...)))
	end
	io.write '\n'
end

local function assert_and_show(a, b, c)
	print(a, '->', b)
	if b ~= c then
		print(b, 'did not equal', c)
		print(debug.traceback())
		error("FAIL!")
	end
end

local function arr_to_set(arr)
	local set = {}
	for _, val in ipairs(arr) do
		set[val] = true
	end
	return set
end

-- Not very efficient.
local function set_eq(arr1, arr2)
	local set1, set2 = arr_to_set(arr1), arr_to_set(arr2)
	for k in pairs(set1) do
		if not set2[k] then
			return false
		end
	end
	for k in pairs(set2) do
		if not set1[k] then
			return false
		end
	end
	return true
end

local function assert_error(...)
	assert(pcall(...) == false)
end

-- Unicode categories
assert(utf8proc.cat 'a' == 'Ll')
assert(utf8proc.catdesc 'a' == 'Letter, lowercase')

local categories = utf8proc.categories
assert(categories.Ll == 'Letter, lowercase')
assert(categories['Letter, lowercase'] == 'Ll')

str1 = 'αὐτός' -- '\u{3B1}\u{1F50}\u{3C4}\u{3CC}\u{3C2}'
-- '\u{3B1}\u{3C5}\u{313}\u{3C4}\u{3BF}\u{301}\u{3C2}'

-- Check comp or decomp and normalize functions.
print('decomposition (NFD):')
assert_and_show(str1, utf8proc.decomp(str1), 'αὐτός')
assert_and_show(str1, utf8proc.decomp(str1), utf8proc.normalize(str1, 'NFD'))

print('composition (NFC):')
local decomposed_str1 = utf8proc.decomp(str1)
assert_and_show(decomposed_str1, utf8proc.normalize(decomposed_str1, 'NFC'),
	str1)
assert_and_show(decomposed_str1,
	utf8proc.normalize(decomposed_str1, 'NFC'),
	utf8proc.comp(decomposed_str1))

print('compatibility decomposition (NFKD):')
assert_and_show('ǆ', utf8proc.cdecomp 'ǆ', 'dž') -- '\u{1C6}' -> '\u{64}\u{7A}\u{30C}'
assert_and_show('ǆ', utf8proc.cdecomp 'ǆ', utf8proc.normalize('ǆ', 'NFKD'))

print('compatibility composition (NFKC):')
assert_and_show('ǆ', utf8proc.ccomp 'ǆ', 'dž') -- '\u{1C6}' -> '\u{64}\u{17E}'
assert_and_show('ǆ', utf8proc.ccomp 'ǆ', utf8proc.normalize('ǆ', 'NFKC'))

str2 = 'ΑΥΤΟΣ' -- '\u{391}\u{3A5}\u{3A4}\u{39F}\u{3A3}'
print('to lowercase:')
assert_and_show(str2, utf8proc.lower(str2),  'αυτοσ')
print('to uppercase:')
assert_and_show('ǆ',  utf8proc.upper 'ǆ', 'Ǆ') -- '\u{1C6}' -> '\u{1C4}'
print('to titlecase:')
assert_and_show('ǆ',  utf8proc.title 'ǆ', 'ǅ') -- '\u{1C6}' -> '\u{1C5}'

if options then -- bit-based options; not currently used
	-- Check map.
	print("bit-based options")

	-- set bitflags as global variables
	for name, option in pairs(type(options) == 'function' and options() or options) do
		if type(name) == 'string' then _ENV[name] = option end
	end
	
	print("lumping:")
	assert_and_show('‘Hello,\u{A0}world!’',
		utf8proc.map('‘Hello,\u{A0}world!’', lump), "'Hello, world!'")
	
	print("diacritic stripping:")
	assert_and_show('αὐτός', utf8proc.map('αὐτός', stripmark | decompose), 'αυτος')
	print("diacritic stripping and case folding:")
	assert_and_show('αὐτός',
		utf8proc.map('αὐτός', stripmark | decompose | casefold), 'αυτοσ')

	assert(utf8proc.map('αὐτός', charbound) ==
		'\xFF\u{3B1}\xFF\u{3C5}\u{313}\xFF\u{3C4}\xFF\u{3BF}\u{301}\xFF\u{3C2}')

	local decomp = utf8proc.decomp(str1)
	print("grapheme breaks:")
	print(decomp, '->', (utf8proc.map(decomp, charbound):gsub('\xFF', '  ')))

	assert(utf8proc.map('αὐτός\0blah', nullterm | stable | decompose) ==
		utf8proc.decomp 'αὐτός\0blah')

	-- Check option parsing function.
	assert(set_eq({ utf8proc.interpret_options(stripmark | decompose | casefold) },
		{ 'stripmark', 'decompose', 'casefold' }))
	-- nlf2lf == nlf2ls | nlf2ps
	assert(set_eq({ utf8proc.interpret_options(nlf2lf | nlf2ls | nlf2ps) }, { 'nlf2lf' }))
	-- Invalid option error.
	local invalid_option = 1 << 14
	assert_error(utf8proc.interpret_options, invalid_option)

	-- Invalid option error.
	assert_error(utf8proc.map, 'abc', invalid_option)
	-- No-option error.
	assert_error(utf8proc.map, 'abc', 0)

	-- Check map_custom.
	assert(utf8proc.map_custom('abc', 0, function (cp) return cp - 0x20 end) == 'ABC')
	assert(utf8proc.map_custom('abc', 0, function () end) == 'abc')
	-- Check error on return type.
	assert_error(utf8proc.map_custom, 'abc', 0, function () return print end)
	
else -- string-based options
	print("string-based options")
	print("lumping:")
	assert_and_show('‘Hello, world!’',
		utf8proc.map('‘Hello, world!’', 'lump'), '\'Hello, world!\'')
	
	print("diacritic stripping:")
	assert_and_show('αὐτός', utf8proc.map('αὐτός', 'stripmark', 'decompose'), 'αυτος')
	print("diacritic stripping and case folding:")
	assert_and_show('αὐτός',
		utf8proc.map('αὐτός', 'stripmark',  'decompose',  'casefold'), 'αυτοσ')

	assert(utf8proc.map('αὐτός', 'charbound') ==
		'\xFF\u{3B1}\xFF\u{3C5}\u{313}\xFF\u{3C4}\xFF\u{3BF}\u{301}\xFF\u{3C2}')
	
	-- Show grapheme divisions.
	local decomp = utf8proc.decomp(str1)
	print("grapheme breaks:")
	print(decomp, '->', (utf8proc.map(decomp, 'charbound'):gsub('\xFF', '  ')))

	assert(utf8proc.map('αὐτός\0blah', 'nullterm',  'stable',  'decompose') ==
		utf8proc.decomp 'αὐτός\0blah')

	-- Invalid option error.
	assert_error(utf8proc.map, 'abc', 'blah')
	-- Conflicting or duplicate option error.
	assert_error(utf8proc.map, '\n', 'nlf2ps', 'nlf2ls')
	assert_error(utf8proc.map, '\u{c2}', 'decompose', 'decompose')
	-- No-option error.
	assert_error(utf8proc.map, 'abc')
	-- Invalid option error.
	assert_error(utf8proc.map, 'abc', function () return end, 'blah')

	-- Check map_custom.
	assert(utf8proc.map_custom('abc', function (cp) return cp - 0x20 end) == 'ABC')
	assert(utf8proc.map_custom('abc', function () end) == 'abc')
	-- Check for error on return type.
	assert_error(utf8proc.map_custom, 'abc', function () return print end)
	-- Check for invalid option error.
	assert_error(utf8proc.map_custom, 'abc', function () return end, 'blah')
	
	-- Check case-insensitivity of option strings.
	assert(pcall(utf8proc.map, '\u{3B1}\u{1F50}\u{3C4}\u{3CC}\u{3C2}', 'DECOMPOSE', 'CHARBOUND') == true)
end

print 'Success!'

f:close()