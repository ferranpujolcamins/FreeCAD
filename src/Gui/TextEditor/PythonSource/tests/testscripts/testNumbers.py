#from https://docs.python.org/3/reference/lexical_analysis.html
#Some examples of integer literals:
7     2147483647                        0177
3L    79228162514264337593543950336L    0377L   0x100000000L # valid in py2, long specifier not valid in py3
      79228162514264337593543950336             0xdeadbeef
      
0b01101

#Some examples of floating point literals:
3.14    10.    .001    1e100    3.14e-10    0e0    3.14_15_93

#imaginary
3.14j   10.j    10j     .001j   1e100j   3.14e-10j   3.14_15_93j

#illegal
1x01    0b1234 12af 0o19
