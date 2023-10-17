# test_module
Dynamisch ladbares Linux Kernel Modul, welches ein Interface für den Userspace bereitstellt, Text austauscht und pro Sekunde ein gesendetes Wort loggt.

# Testumgebung (Software)
Linux Kernel Release:  
5.15.0-86-generic

Betriebssystem (virtualisiert):  
Ubuntu 22.04.3 LTS Server for ARM mit ubuntu-desktop Paketen und Parallels Tools  
[https://cdimage.ubuntu.com/releases/22.04/release/ubuntu-22.04.3-live-server-arm64.iso](https://cdimage.ubuntu.com/releases/22.04/release/ubuntu-22.04.3-live-server-arm64.iso)

Virtualisierungssoftware:  
Parallels ® Desktop 19 für Mac  
[https://www.parallels.com/de/products/desktop/](https://www.parallels.com/de/products/desktop/)


# Kompilieren
```
git clone https://github.com/Felix10R/test_module.git
cd test_module
make
cc userspace_app.c -o userspace_app
```

# Verwendung
```
sudo insmod test_module.ko
sudo ./userspace_app
```

# Genutzte Quellen
Einstieg und Makefile:  
[https://sysprog21.github.io/lkmpg/](https://sysprog21.github.io/lkmpg/)

Datenaustausch mit dem Userspace (allgemeines Vorgehen und Userspace Demo App):  
[https://www.youtube.com/watch?v=DwwZR0EE_1A](https://www.youtube.com/watch?v=DwwZR0EE_1A)

Kernel API (indication errors, printk() help functions, kmalloc, mutex):  
[https://linux-kernel-labs.github.io/refs/heads/master/labs/kernel_api.html](https://linux-kernel-labs.github.io/refs/heads/master/labs/kernel_api.html)

Memory Management:  
[https://www.chiark.greenend.org.uk/doc/linux-doc-3.16/html/kernel-api/mm.html](https://www.chiark.greenend.org.uk/doc/linux-doc-3.16/html/kernel-api/mm.html)

Linked Lists:  
[https://embetronicx.com/tutorials/linux/device-drivers/linux-device-driver-tutorial-part-17-linked-list-in-linux-kernel/](https://embetronicx.com/tutorials/linux/device-drivers/linux-device-driver-tutorial-part-17-linked-list-in-linux-kernel/)

Workqueues:  
[https://linux-kernel-labs.github.io/refs/heads/master/labs/deferred_work.html#workqueues](https://linux-kernel-labs.github.io/refs/heads/master/labs/deferred_work.html#workqueues)