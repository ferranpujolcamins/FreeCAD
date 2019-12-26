# from https://docs.python.org/3/reference/lexical_analysis.html

"a string"
'a sgl quote string'
"\s a escaped \b\r\n"
"\"still a string\""
'\' still a valid string\''
"""a long string"""
'''another long string'''

"""multiline
string
here"""

#bytes string
b"a byte string, ASCCI only"
b'a sgl byte string'
B"also a bytes string"
B"a sgl BYTES string"
BR"a raw BYTES string"
BR'a raw sql BYTES string'
Br"a raw BYTES string r small"
Br'a raw sql BYTES string r small'
bR"a raw BYTES string b small"
bR'a raw sql BYTES string b small'
br"a raw bytes string"
br'a raw sgl bytes string'
# valid from 3.3 (reverse prefix chars)
RB"a raw BYTES string"
RB'a raw sql BYTES string'
rB"a raw BYTES string r small"
rB'a raw sql BYTES string r small'
Rb"a raw BYTES string b small"
Rb'a raw sql BYTES string b small'
rb"a raw bytes string"
rb'a raw sgl bytes string'

#string unicode in py3
u"a string"
u'a sgl string'
U'a sgl STRING'
U"a STRING"

#format string
f"format string"
f'format sgl string'
F"format STRING"
F'format sgl STRING'
fr"format raw string"
fr'format raw sgl string'
Fr"FORMAT raw string"
Fr'FORMAT raw sgl string'
fR"format RAW string"
fR'format RAW sgl string'
FR"FORMAT RAW string"
FR'FORMAT RAW sgl string'

# longstrings prefixes
#bytes string
b"""a byte string, ASCCI only"""
b'''a sgl byte string'''
B"""also a bytes string"""
B"""a sgl BYTES string"""
BR"""a raw BYTES string"""
BR'''a raw sql BYTES string'''
Br"""a raw BYTES string r small"""
Br'''a raw sql BYTES string r small'''
bR"""a raw BYTES string b small"""
bR'''a raw sql BYTES string b small'''
br"""a raw bytes string"""
br'''a raw sgl bytes string'''
# valid from 3.3 (reverse prefix chars)
RB"""a raw BYTES string"""
RB'''a raw sql BYTES string'''
rB"""a raw BYTES string r small"""
rB'''a raw sql BYTES string r small'''
Rb"""a raw BYTES string b small"""
Rb'''a raw sql BYTES string b small'''
rb"""a raw bytes string"""
rb'''a raw sgl bytes string'''

#string unicode in py3
u"""a string"""
u'''a sgl string'''
U'''a sgl STRING'''
U"""a STRING"""

#format string
f"""format string"""
f'''format sgl string'''
F"""format STRING"""
F'''format sgl STRING'''
fr"""format raw string"""
fr'''format raw sgl string'''
Fr"""FORMAT raw string"""
Fr'''FORMAT raw sgl string'''
fR"""format RAW string"""
fR'''format RAW sgl string'''
FR"""FORMAT RAW string"""
FR'''FORMAT RAW sgl string'''


f"He said his name is {name!r}."
f"He said his name is {repr(name)}."  # repr() is equivalent to !r
f"result: {value:{width}.{precision}}"  # nested fields
f"{today:%B %d, %Y}"  # using date format specifier
f"{number:#0x}"  # using integer format specifier
f"newline: {ord('\n')}"  # raises SyntaxError due to excape in format string,

"\U0435 unicodesequence"

