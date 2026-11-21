// Build: aarch64-linux-gnu-gcc -static -Os -Wall -Wextra -o touch-window touch-window.c
#include <dirent.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

static long long milliseconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_BOOTTIME, &t);
    return 1000LL * t.tv_sec + t.tv_nsec / 1000000;
}

static int open_input(const char *name) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return -1;
    int fd = -1;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "event", 5)) continue;
        fd = openat(dirfd(dir), entry->d_name, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        char id[128] = {0};
        if (fd >= 0 && ioctl(fd, EVIOCGNAME(sizeof id), id) >= 0 && !strcmp(id,name)) break;
        if (fd >= 0) close(fd);
        fd = -1;
    }
    closedir(dir);
    return fd;
}

static const int axes[] = { ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_TRACKING_ID,
    ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TOUCH_MAJOR, ABS_MT_TOUCH_MINOR };

static int create_panel(int source) {
    int fd = open("/dev/uinput", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    struct uinput_user_dev d = {0};
    strcpy(d.name, "Local PIN touchscreen");
    d.id.bustype = BUS_HOST;
    if (ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT) < 0 ||
        ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0 || ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) < 0 || ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_FINGER) < 0) goto fail;
    for (size_t i=0; i<sizeof axes/sizeof axes[0]; ++i) {
        int axis = axes[i];
        struct input_absinfo a;
        if (ioctl(source, EVIOCGABS(axis), &a) < 0 || ioctl(fd, UI_SET_ABSBIT, axis) < 0) goto fail;
        d.absmin[axis]=a.minimum; d.absmax[axis]=a.maximum;
        d.absfuzz[axis]=a.fuzz; d.absflat[axis]=a.flat;
    }
    if (write(fd,&d,sizeof d) == sizeof d && ioctl(fd,UI_DEV_CREATE) == 0) return fd;
fail:
    close(fd);
    return -1;
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);
    struct pollfd inputs[] = {{open_input("qpnp_pon"),POLLIN,0},
                              {open_input("gpio-keys"),POLLIN,0},{-1,POLLIN,0}};
    if (inputs[0].fd < 0 || inputs[1].fd < 0) return 1;
    const int sequence[] = {KEY_POWER, KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_VOLUMEUP};
    unsigned step=0;
    int held=0, panel=-1;
    long long started=0, pressed=0, expires=0;
    for (;;) {
        long long now=milliseconds();
        if (panel>=0 && now>=expires) {
            close(panel); close(inputs[2].fd);
            panel=inputs[2].fd=-1;
        }
        int ready=poll(inputs,3,panel<0 ? -1 : (int)(expires-now));
        if (ready<0) return 1;
        for (int channel=0; channel<3; ++channel) {
            if (inputs[channel].fd<0 || !inputs[channel].revents) continue;
            if (inputs[channel].revents != POLLIN) return 1;
            struct input_event events[32];
            ssize_t n=read(inputs[channel].fd,events,sizeof events);
            if (n<=0) return 1;
            for (size_t i=0; i<(size_t)n/sizeof events[0]; ++i) {
                struct input_event *e=&events[i];
                now=milliseconds();
                if (channel==2) {
                    if (now>=expires) break;
                    if (e->type==EV_SYN && e->code==SYN_DROPPED) return 1;
                    if (e->type==EV_ABS || e->type==EV_SYN ||
                        (e->type==EV_KEY && (e->code==BTN_TOUCH || e->code==BTN_TOOL_FINGER))) {
                        if (write(panel,e,sizeof *e)!=sizeof *e) return 1;
                    }
                    continue;
                }
                if (e->type!=EV_KEY || (channel==0 && e->code!=KEY_POWER && e->code!=KEY_VOLUMEDOWN) ||
                    (channel==1 && e->code!=KEY_VOLUMEUP)) continue;
                if (panel>=0) {
                    if (e->code==KEY_POWER && e->value==1) expires=now;
                    continue;
                }
                if (e->value==1) {
                    if (held) { step=0; held=-1; }
                    else { held=e->code; pressed=now; }
                    continue;
                }
                if (e->value!=0) continue;
                if (held!=e->code || now-pressed>700) { held=0; step=0; continue; }
                held=0;
                if (now-started>5000 || e->code!=sequence[step]) step=0;
                if (e->code!=sequence[step]) continue;
                if (!step) started=pressed;
                if (++step<4) continue;
                step=0;
                inputs[2].fd=open_input("synaptics_dsx");
                if (inputs[2].fd<0 || (panel=create_panel(inputs[2].fd))<0) return 1;
                expires=now+30000;
                if (fork()==0) {
                    execl("/system/bin/input","input","keyevent","KEYCODE_WAKEUP",(char *)NULL);
                    _exit(1);
                }
            }
        }
    }
}
