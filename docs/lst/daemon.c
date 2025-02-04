#include <syslog.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <linux/input.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <gio/gio.h>

#define LOCKFILE "/var/run/daemon.pid"
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define DATA_SIZE_BYTES 4

#define MIN_BRIGHTNESS  193
#define MAX_BRIGHTNESS  19393
#define STP_BRIGHTNESS  1

#define MIN_TEMPERATURE 1700
#define MAX_TEMPERATURE 4700
#define STP_TEMPERATURE 20

#define ACELINE_BUTTON_LEFT 0b00000001
#define ACELINE_BUTTON_RGHT 0b00000010
#define ACELINE_BUTTON_MDDL 0b00000100
#define ACELINE_SCROLL_UP   0b00000001
#define ACELINE_SCROLL_DOWN '\xff'

#define GET_NLTEMP_COMMAND  "sudo -H -u sheglar DISPLAY=:0 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus gsettings get org.gnome.settings-daemon.plugins.color night-light-temperature > /home/sheglar/.aceline.tmp"
#define GET_NLTEMP_VALUE    "sudo tail -c 5 /home/sheglar/.aceline.tmp > /home/sheglar/.aceline.tmp1"
#define SET_NLTEMP_COMMAND  "sudo -H -u sheglar DISPLAY=:0 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus gsettings set org.gnome.settings-daemon.plugins.color night-light-temperature %d"
#define ACELINE_FILENAME    "/dev/aceline_mouse"
#define NLTEMP_FILENAME     "/home/sheglar/.aceline.tmp1"
#define BRIGHTNESS_FILENAME "/sys/class/backlight/intel_backlight/brightness"

int lockfile(int fd)
{
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    return(fcntl(fd, F_SETLK, &fl));
}

int already_running(void)
{
    int fd;
    char buf[16];

    fd = open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE);
    if (fd < 0)
    {
        syslog(LOG_ERR, "невозможно открыть %s: %s", LOCKFILE, strerror(errno));
        exit(1);
    }
    if (lockfile(fd) < 0)
    {
        if (errno == EACCES || errno == EAGAIN)
        {
            close(fd);
            return(1);
        }
        syslog(LOG_ERR, "невозможно установить блокировку на %s: %s", LOCKFILE, strerror(errno));
        exit(1);
    }
    ftruncate(fd, 0);

    return 0;
}

void daemonize(const char *cmd)
{
    int i, fd0, fd1, fd2;
    pid_t pid;
    struct rlimit r1;
    struct sigaction sa;

    umask(0);

    if (getrlimit(RLIMIT_NOFILE, &r1) < 0)
    {
        perror("Невозможно получить макс номер дескриптора");
        exit(1);
    }

    if ((pid = fork()) < 0)
    {
        perror("Fork error");
        exit(1);
    }
    else if (pid != 0)
        exit(0);
    setsid();
    
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
    {
        perror("Cant ignore sighup");
        exit(1);
    }

    if ((pid = fork()) < 0)
    {
        perror("Fork error");
        exit(1);
    }
    else if (pid != 0)
        exit(0);

    if (chdir("/") < 0)
    {
        perror("Error");
        exit(1);
    }
    
    if (r1.rlim_max == RLIM_INFINITY)
        r1.rlim_max = 1024;
    for (i=0; i<r1.rlim_max; i++)
        close(i);

    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2)
    {
        syslog(LOG_ERR, "ошибочные файловые дескрипторы %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

void change_brightness(int direction)
{
    if (!direction)
        return;

    FILE *brightness_fd = fopen(BRIGHTNESS_FILENAME, "r");

    int cur_value = MIN_BRIGHTNESS;
    fscanf(brightness_fd, "%d", &cur_value);
    int new_value = cur_value + STP_BRIGHTNESS * direction;

    if (new_value < MIN_BRIGHTNESS)
        new_value = MIN_BRIGHTNESS;
    else if (new_value > MAX_BRIGHTNESS)
        new_value = MAX_BRIGHTNESS;


    brightness_fd = fopen(BRIGHTNESS_FILENAME, "w");
    fseek(brightness_fd, 0, SEEK_SET);
    fprintf(brightness_fd, "%d\n", new_value);
    fclose(brightness_fd);
}

void change_nltemp(int direction)
{
    if (!direction)
        return;

    system(GET_NLTEMP_COMMAND);
    system(GET_NLTEMP_VALUE);

    FILE *fd = fopen(NLTEMP_FILENAME, "r");
    int cur_value = MIN_TEMPERATURE;
    fscanf(fd, "%d", &cur_value);
    fclose(fd);

    int new_value = cur_value + STP_TEMPERATURE * direction;

    if (new_value < MIN_TEMPERATURE)
        new_value = MIN_TEMPERATURE;
    else if (new_value > MAX_TEMPERATURE)
        new_value = MAX_TEMPERATURE;

    char command[200];
    snprintf(command, 200, SET_NLTEMP_COMMAND, new_value);
    system(command);
}

int get_brightness_direction(char *buffer)
{
    if (!(buffer[0] ^ ACELINE_BUTTON_LEFT))
        return 1;

    if (!(buffer[0] ^ ACELINE_BUTTON_RGHT))
        return -1;

    return 0;
}

int get_nltemp_direction(char *buffer)
{
    if (!(buffer[3] ^ ACELINE_SCROLL_UP))
        return -1;

    if (buffer[3] == ACELINE_SCROLL_DOWN)
        return 1;

    return 0;
}

int main()
{
    daemonize("ddaceline");
    syslog(LOG_WARNING, "@ DAEMON daemonize success\n");

    if (already_running() != 0) {
        syslog(LOG_ERR, "@ DAEMON already_running\n");
        exit(1);
    }

    syslog(LOG_WARNING, "@ DAEMON already_running success\n");
    syslog(LOG_WARNING, "@ DAEMON start process\n");
    syslog(LOG_WARNING, "@ DAEMON pid=%d\n", getpid());

    int aceline_fd = open(ACELINE_FILENAME, O_RDONLY);

    if (aceline_fd < 0) {
        syslog(LOG_ERR, "@ DAEMON open /dev/aceline_mouse error: %s\n", strerror(errno));
        return -1;
    }

    syslog(LOG_WARNING, "@ DAEMON open aceline_mouse success\n");

    while (1) {
        unsigned char buffer[DATA_SIZE_BYTES];
        ssize_t ret = read(aceline_fd, buffer, DATA_SIZE_BYTES);
        int d_brightness = get_brightness_direction(buffer);

        if (d_brightness != 0) {
            change_brightness(d_brightness);
        }

        int d_nltemp = get_nltemp_direction(buffer);

        if (d_nltemp != 0) {
            change_nltemp(d_nltemp);
        }
    }
}
