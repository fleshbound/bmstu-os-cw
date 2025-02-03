# Как загрузить


```
make
```

```
sudo insmod aceline.ko
```

```
echo "3-7:1-0" | sudo tee /sys/bus/usb/drivers/usbhid/unbind
```

```
echo "3-7:1-0" | sudo tee /sys/bus/usb/drivers/aceline\_mouse/bind
```

```
gcc `pkg-config --cflags glib-2.0` daemon.c `pkg-config --libs glib-2.0` `pkg-config --libs gtk+-2.0` -o daemon
```

```
sudo ./daemon
```

После завершения работы

```
echo "3-7:1.0" | sudo tee /sys/bus/usb/drivers/usbhid/bind
```
