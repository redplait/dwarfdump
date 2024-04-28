Dirty patches for gcc12

c-attribs.patch - to add params direction (like param_in/param_out). [details](http://redplait.blogspot.com/2023/04/custom-attributes-in-gcc-and-dwarf.html)

dwarf2out.patch - to write custom dwarf attributes from above patch to debug info

final.patch - to skip profiling functions located in section ".init.text"

varasm.patch - to place string literals in arbitrary section. [details](http://redplait.blogspot.com/2024/04/gcc-placing-strings-in-arbitrary-section.html)
