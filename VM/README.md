# GC
使用mark-sweep算法,测试了一下，效率不说，至少还是能正确GC的


# 编译过程
因为自己实现ffi太麻烦了，要考虑不同CPU和编译器的ABI，所以使用了libffi这个库，导致vm项目编译起来略微麻烦,当然如果自己已经搞定libffi的安装，也可以忽略下面教程  
下面教程是windows上使用msvc编译成64位程序，不支持32位程序
1. 安装vcpkg(如果安装不了,可以用"set HTTPS_PROXY=127.0.0.1:7890"设置代理)
1. 执行"vcpkg integrated install"使vcpkg的包集成到全局[参考链接](https://www.bilibili.com/read/cv15439255/)
1. 执行"vcpkg install libffi --triplet x64-windows-static"安装libffi *(需要注意的是,通过这种方式使用的libffi有点问题,我自己用的时候发现vs会自动把libffi中的"debug/bin/libffi.dll复制到vm.exe目录下,这个dll会崩溃,实际上用libffi中的/bin/libffi.dll就正常，所以需要手工把这个dll覆盖掉vs自动复制的dll")*