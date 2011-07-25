/* Let the linker automatically allocate the .initcall.init
 * (orphan) section for us. We just need to find out the
 * address of the section.
 * */
__initcall_start = LOADADDR(.initcall.init);
__initcall_end = __initcall_start + SIZEOF(.initcall.init);
