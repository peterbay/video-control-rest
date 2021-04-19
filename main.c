/*
 * video-control-rest
 * 
 * Petr Vavrin (peterbay)   pvavrin@gmail.com
 *                          https://github.com/peterbay
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "mongoose.h"
#include "mjson.h"

#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static int debug_enabled = 0;
static char *getFormattedTime(void);

#define pixfmtstr(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

#define __LOG__(format, loglevel, ...) printf("%s %-5s " format "\n", getFormattedTime(), loglevel, ##__VA_ARGS__)

#define LOGDEBUG(format, ...)                    \
    if (debug_enabled)                           \
    {                                            \
        __LOG__(format, "DEBUG", ##__VA_ARGS__); \
    }
#define LOGWARN(format, ...) __LOG__(format, "WARN", ##__VA_ARGS__)
#define LOGERROR(format, ...) __LOG__(format, "ERROR", ##__VA_ARGS__)
#define LOGINFO(format, ...) __LOG__(format, "INFO", ##__VA_ARGS__)

#define URL_DEVICES "/devices"
#define URL_DEVICE_FORMATS "/device/formats/*"
#define URL_DEVICE_FORMAT "/device/format/*"
#define URL_DEVICE_CONTROL "/device/control/*"

#define FORMAT_CONTROL_VALUE "\"%s\": %d"
#define FORMAT_CONTROL_STRING "\"%s\": \"%s\""
#define FORMAT_DISCRETE "\"%c%c%c%c\": { \"type\": \"DISCRETE\", \"width\": \"%d\", \"height\": \"%d\" }"
#define FORMAT_STEPWISE "\"%c%c%c%c\": { \"type\": \"STEPWISE\", \"min_width\": \"%d\", \"min_height\": \"%d\", \"max_width\": \"%d\", \"max_height\": \"%d\", \"step_width\": \"%d\", \"step_height\": \"%d\" }"
#define FORMAT_DEVICE_CAPABILITIES "\"%s\": { \"driver\": \"%s\", \"card\": \"%s\", \"bus_info\": \"%s\", \"version\": \"%d\", \"capabilities\": [ %.*s ] }\n"
#define FORMAT_CONTROL "\"%s\": { \"minimum\": \"%d\", \"maximum\": \"%d\", \"default\": \"%d\", \"step\": \"%d\", \"value\": \"%d\", \"menu\": { "
#define FORMAT_PIX_FORMAT "\"pix\": { \"width\": \"%d\", \"height\": \"%d\", \"pixelformat\": \"%c%c%c%c\", \"field\": \"%s\", \"bytesperline\": \"%d\", \"sizeimage\": \"%d\", \"colorspace\": \"%s\", \"priv\": \"%d\", \"flags\": \"%d\" }"

static int s_signo;
static void signal_handler(int signo)
{
    s_signo = signo;
}

static char *listen_port = "8800";
static char *listen_ip = "0.0.0.0";
static char s_listen_on[128] = {'\0'};

enum http_methods
{
    METHOD_GET = 1,
    METHOD_POST
};

struct enum_names
{
    int bitmask;
    char *name;
};

struct enum_names v4l2_capability_names[23] = {
    {V4L2_CAP_VIDEO_CAPTURE, "VIDEO_CAPTURE"},
    {V4L2_CAP_VIDEO_OUTPUT, "VIDEO_OUTPUT"},
    {V4L2_CAP_VIDEO_OVERLAY, "VIDEO_OVERLAY"},
    {V4L2_CAP_VBI_CAPTURE, "VBI_CAPTURE"},
    {V4L2_CAP_VBI_OUTPUT, "VBI_OUTPUT"},
    {V4L2_CAP_SLICED_VBI_CAPTURE, "SLICED_VBI_CAPTURE"},
    {V4L2_CAP_SLICED_VBI_OUTPUT, "SLICED_VBI_OUTPUT"},
    {V4L2_CAP_RDS_CAPTURE, "RDS_CAPTURE"},
    {V4L2_CAP_VIDEO_OUTPUT_OVERLAY, "VIDEO_OUTPUT_OVERLAY"},
    {V4L2_CAP_HW_FREQ_SEEK, "HW_FREQ_SEEK"},
    {V4L2_CAP_RDS_OUTPUT, "RDS_OUTPUT"},
    {V4L2_CAP_VIDEO_CAPTURE_MPLANE, "VIDEO_CAPTURE_MPLANE"},
    {V4L2_CAP_VIDEO_OUTPUT_MPLANE, "VIDEO_OUTPUT_MPLANE"},
    {V4L2_CAP_VIDEO_M2M_MPLANE, "VIDEO_M2M_MPLANE"},
    {V4L2_CAP_VIDEO_M2M, "VIDEO_M2M"},
    {V4L2_CAP_TUNER, "TUNER"},
    {V4L2_CAP_AUDIO, "AUDIO"},
    {V4L2_CAP_RADIO, "RADIO"},
    {V4L2_CAP_MODULATOR, "MODULATOR"},
    {V4L2_CAP_READWRITE, "READWRITE"},
    {V4L2_CAP_ASYNCIO, "ASYNCIO"},
    {V4L2_CAP_STREAMING, "STREAMING"},
    {V4L2_CAP_DEVICE_CAPS, "DEVICE_CAPS"}};

struct enum_names v4l2_buffer_type_names[23] = {
    {V4L2_BUF_TYPE_VIDEO_CAPTURE, "VIDEO_CAPTURE"},
    {V4L2_BUF_TYPE_VIDEO_OUTPUT, "VIDEO_OUTPUT"},
    {V4L2_BUF_TYPE_VIDEO_OVERLAY, "VIDEO_OVERLAY"},
    {V4L2_BUF_TYPE_VBI_CAPTURE, "VBI_CAPTURE"},
    {V4L2_BUF_TYPE_VBI_OUTPUT, "VBI_OUTPUT"},
    {V4L2_BUF_TYPE_SLICED_VBI_CAPTURE, "SLICED_VBI_CAPTURE"},
    {V4L2_BUF_TYPE_SLICED_VBI_OUTPUT, "SLICED_VBI_OUTPUT"},
    {V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY, "VIDEO_OUTPUT_OVERLAY"},
    {V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, "VIDEO_CAPTURE_MPLANE"},
    {V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, "VIDEO_OUTPUT_MPLANE"},
    {V4L2_BUF_TYPE_SDR_CAPTURE, "SDR_CAPTURE"},
    {V4L2_BUF_TYPE_SDR_OUTPUT, "SDR_OUTPUT"}};

struct enum_names v4l2_ctrl_flags[11] = {
    {V4L2_CTRL_FLAG_DISABLED, "DISABLED"},
    {V4L2_CTRL_FLAG_GRABBED, "GRABBED"},
    {V4L2_CTRL_FLAG_READ_ONLY, "READ_ONLY"},
    {V4L2_CTRL_FLAG_UPDATE, "UPDATE"},
    {V4L2_CTRL_FLAG_INACTIVE, "INACTIVE"},
    {V4L2_CTRL_FLAG_SLIDER, "SLIDER"},
    {V4L2_CTRL_FLAG_WRITE_ONLY, "WRITE_ONLY"},
    {V4L2_CTRL_FLAG_VOLATILE, "VOLATILE"},
    {V4L2_CTRL_FLAG_HAS_PAYLOAD, "HAS_PAYLOAD"},
    {V4L2_CTRL_FLAG_EXECUTE_ON_WRITE, "EXECUTE_ON_WRITE"},
    {V4L2_CTRL_FLAG_MODIFY_LAYOUT, "MODIFY_LAYOUT"}};

struct enum_names v4l2_field_names[10] = {
    {V4L2_FIELD_ANY, "ANY"},
    {V4L2_FIELD_NONE, "NONE"},
    {V4L2_FIELD_TOP, "TOP"},
    {V4L2_FIELD_BOTTOM, "BOTTOM"},
    {V4L2_FIELD_INTERLACED, "INTERLACED"},
    {V4L2_FIELD_SEQ_TB, "SEQ_TB"},
    {V4L2_FIELD_SEQ_BT, "SEQ_BT"},
    {V4L2_FIELD_ALTERNATE, "ALTERNATE"},
    {V4L2_FIELD_INTERLACED_TB, "INTERLACED_TB"},
    {V4L2_FIELD_INTERLACED_BT, "INTERLACED_BT"}};

struct enum_names v4l2_colorspace_names[13] = {
    {V4L2_COLORSPACE_DEFAULT, "DEFAULT"},
    {V4L2_COLORSPACE_SMPTE170M, "SMPTE170M"},
    {V4L2_COLORSPACE_SMPTE240M, "SMPTE240M"},
    {V4L2_COLORSPACE_REC709, "REC709"},
    {V4L2_COLORSPACE_BT878, "BT878"},
    {V4L2_COLORSPACE_470_SYSTEM_M, "470_SYSTEM_M"},
    {V4L2_COLORSPACE_470_SYSTEM_BG, "SYSTEM_BG"},
    {V4L2_COLORSPACE_JPEG, "JPEG"},
    {V4L2_COLORSPACE_SRGB, "SRGB"},
    {V4L2_COLORSPACE_OPRGB, "OPRGB"},
    {V4L2_COLORSPACE_BT2020, "BT2020"},
    {V4L2_COLORSPACE_RAW, "RAW"},
    {V4L2_COLORSPACE_DCI_P3, "DCI_P3"}};

static char *getFormattedTime(void)
{

    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    static char _retval[20];
    strftime(_retval, sizeof(_retval), "%Y-%m-%d %H:%M:%S", timeinfo);

    return _retval;
}

int digits_only(const char *s)
{
    while (*s)
    {
        if (isdigit(*s++) == 0)
            return 0;
    }

    return 1;
}

static char *name2var(char *name)
{
    int i;
    int len_name = strlen(name) - 1;
    char lowercase;
    char out_name[128] = {'\0'};
    bool add_underscore = false;

    if (len_name > 127)
    {
        return NULL;
    }

    for (i = 0; i <= len_name; i++)
    {
        if (isalnum(name[i]))
        {
            if (add_underscore)
            {
                strcat(out_name, "_");
                add_underscore = false;
            }
            lowercase = tolower(name[i]);
            strncat(out_name, &lowercase, 1);
        }
        else
        {
            add_underscore = true;
        }
    }
    return strdup((const char *)out_name);
}

static void device_list(struct mg_connection *con)
{
    int fd;
    struct dirent *ep;
    struct v4l2_capability cap;
    char path[80];
    char device[1024] = {'\0'};
    char devices[8096] = {'\0'};
    char capabilities[1024] = {'\0'};
    int count_capabilities = 0;
    int count_devices = 0;
    int c;

    strcat(devices, "{ ");

    DIR *dp = opendir("/dev");

    if (dp != NULL)
    {
        while ((ep = readdir(dp)))
        {
            if (strncmp(ep->d_name, "video", 5))
            {
                continue;
            }
            strcpy(path, "/dev/");
            strcat(path, ep->d_name);

            fd = open(path, O_RDWR | O_NONBLOCK);
            if (!fd)
            {
                continue;
            }

            device[0] = '\0';
            capabilities[0] = '\0';
            count_capabilities = 0;

            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) > -1)
            {
                for (c = 0; c < 23; c++)
                {
                    if ((cap.capabilities & v4l2_capability_names[c].bitmask))
                    {
                        if (count_capabilities)
                        {
                            strcat(capabilities, ",");
                        }
                        strcat(capabilities, "\"");
                        strcat(capabilities, v4l2_capability_names[c].name);
                        strcat(capabilities, "\"");
                        count_capabilities++;
                    }
                }
                sprintf(device, FORMAT_DEVICE_CAPABILITIES,
                        ep->d_name,
                        cap.driver,
                        cap.card,
                        cap.bus_info,
                        cap.version,
                        strlen(capabilities), capabilities);

                if (count_devices)
                {
                    strcat(devices, ",");
                }
                strcat(devices, device);
                count_devices++;
            }
            close(fd);
        }
        closedir(dp);
    }
    strcat(devices, " }\n");
    mg_http_reply(con, 200, "Content-Type: application/json\r\n", devices);
}

static int device_open(char *device_name)
{
    char path[256];
    if (strncmp(device_name, "video", 5))
    {
        return -ENODEV;
    }
    strcpy(path, "/dev/");
    strcat(path, device_name);

    return open(path, O_RDWR | O_NONBLOCK);
}

static void device_control_get(struct mg_connection *con,
                               char *device_name)
{
    struct v4l2_control ctrl;
    struct v4l2_queryctrl queryctrl;
    struct v4l2_querymenu querymenu;
    char append[1024] = {'\0'};
    char control[1024] = {'\0'};
    char controls[65536] = {'\0'};
    char flags[2048] = {'\0'};
    char menu[2048] = {'\0'};
    char *var_name;
    const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
    int c;
    int controls_count = 0;
    int count_flags = 0;
    int menu_count = 0;
    int menu_index = 0;
    int fd = device_open(device_name);

    if (!fd)
    {
        mg_http_reply(con, 400, "", "Device can't be opened.");
        return;
    }

    memset(&queryctrl, 0, sizeof(struct v4l2_queryctrl));
    memset(&querymenu, 0, sizeof(struct v4l2_querymenu));
    memset(&ctrl, 0, sizeof(struct v4l2_control));
    controls[0] = '\0';
    append[0] = '\0';

    strcat(controls, "{ ");

    queryctrl.id = next_fl;
    while (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0)
    {
        count_flags = 0;
        flags[0] = '\0';
        control[0] = '\0';

        for (c = 0; c < 11; c++)
        {
            if ((queryctrl.flags & v4l2_ctrl_flags[c].bitmask))
            {
                if (count_flags)
                {
                    strcat(flags, ",");
                }
                strcat(flags, "\"");
                strcat(flags, v4l2_ctrl_flags[c].name);
                strcat(flags, "\"");
                count_flags++;
            }
        }

        var_name = name2var((char *)queryctrl.name);
        if (var_name)
        {
            menu[0] = '\0';
            menu_count = 0;

            ctrl.id = queryctrl.id;
            if (ioctl(fd, VIDIOC_G_CTRL, &ctrl) == 0)
            {
                if (queryctrl.type == V4L2_CTRL_TYPE_MENU ||
                    queryctrl.type == V4L2_CTRL_TYPE_INTEGER_MENU)
                {
                    for (menu_index = queryctrl.minimum; menu_index <= queryctrl.maximum; menu_index++)
                    {
                        querymenu.id = queryctrl.id;
                        querymenu.index = menu_index;
                        if (ioctl(fd, VIDIOC_QUERYMENU, &querymenu) == 0)
                        {
                            if (menu_count)
                            {
                                strcat(menu, ", ");
                            }
                            sprintf(append, "%d", querymenu.index);
                            strcat(menu, "\"");
                            strcat(menu, append);
                            strcat(menu, "\": \"");

                            if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
                            {
                                strcat(menu, (char *)querymenu.name);
                            }
                            else
                            {
                                sprintf(append, "%lld", querymenu.value);
                                strcat(menu, append);
                            }
                            strcat(menu, "\"");
                            menu_count++;
                        }
                    }
                }
                sprintf(control, FORMAT_CONTROL,
                        var_name,
                        queryctrl.minimum,
                        queryctrl.maximum,
                        queryctrl.default_value,
                        queryctrl.step,
                        ctrl.value);

                strcat(control, menu);
                strcat(control, " } }");

                if (controls_count)
                {
                    strcat(controls, ", ");
                }
                strcat(controls, control);
                controls_count++;
            }
            free(var_name);
        }
        queryctrl.id |= next_fl;
    }
    close(fd);

    strcat(controls, " }\n");
    mg_http_reply(con, 200, "Content-Type: application/json\r\n", controls);
}

static void device_control_set(struct mg_connection *con,
                               struct mg_http_message *hm,
                               char *device_name)
{
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control ctrl;
    struct v4l2_querymenu querymenu;
    char *var_name;
    char control[1024] = {'\0'};
    char controls[65536] = {'\0'};
    char json_path[256] = {'\0'};
    char buf[32];
    const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
    double dv;
    int controls_count = 0;
    int fd = device_open(device_name);

    if (!fd)
    {
        mg_http_reply(con, 400, "", "Device can't be opened.");
        return;
    }

    memset(&queryctrl, 0, sizeof(struct v4l2_queryctrl));
    memset(&querymenu, 0, sizeof(struct v4l2_querymenu));
    memset(&ctrl, 0, sizeof(struct v4l2_control));
    strcat(controls, "{ ");

    queryctrl.id = next_fl;
    while (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0)
    {
        ctrl.id = queryctrl.id;
        queryctrl.id |= next_fl;

        var_name = name2var((char *)queryctrl.name);
        if (!var_name)
        {
            continue;
        }

        sprintf(json_path, "$.%s", var_name);

        if (mjson_get_number(hm->body.ptr, hm->body.len, json_path, &dv))
        {
            ctrl.value = (__s32)dv;
            LOGDEBUG("Device %s control %s set %d", device_name, var_name, ctrl.value);

            if (ioctl(fd, VIDIOC_S_CTRL, &ctrl) == 0)
            {
                if (ioctl(fd, VIDIOC_G_CTRL, &ctrl) == 0)
                {
                    sprintf(control, FORMAT_CONTROL_VALUE, var_name, ctrl.value);
                    LOGDEBUG("Device %s control %s get %d", device_name, var_name, ctrl.value);
                }
                else
                {
                    sprintf(control, FORMAT_CONTROL_STRING, var_name, strerror(errno));
                    LOGERROR("Device %s control %s: %s", device_name, var_name, strerror(errno));
                }
            }
            else
            {
                sprintf(control, FORMAT_CONTROL_STRING, var_name, strerror(errno));
                LOGERROR("Device %s control %s: %s", device_name, var_name, strerror(errno));
            }

            if (controls_count)
            {
                strcat(controls, ", ");
            }
            strcat(controls, control);
            controls_count++;
        }
        else if (mjson_get_string(hm->body.ptr, hm->body.len, json_path, buf, sizeof(buf)) > -1)
        {
            sprintf(control, FORMAT_CONTROL_STRING, var_name, "Error: Only numbers are expected");
            LOGERROR("Device %s control %s: Only numbers are expected", device_name, var_name);

            if (controls_count)
            {
                strcat(controls, ", ");
            }
            strcat(controls, control);
            controls_count++;
        }
        free(var_name);
    }
    close(fd);
    strcat(controls, " }\n");
    mg_http_reply(con, 200, "Content-Type: application/json\r\n", controls);
}

static int device_buffer_check(struct v4l2_capability *cap, int buffer_index, int exclude_overlay)
{
    switch (buffer_index)
    {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        return (cap->capabilities & V4L2_CAP_VIDEO_CAPTURE);

    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
        return (cap->capabilities & V4L2_CAP_VIDEO_OUTPUT);

    case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        return !exclude_overlay && (cap->capabilities & V4L2_CAP_VIDEO_OVERLAY);

    case V4L2_BUF_TYPE_VBI_CAPTURE:
        return (cap->capabilities & V4L2_CAP_VBI_CAPTURE);

    case V4L2_BUF_TYPE_VBI_OUTPUT:
        return (cap->capabilities & V4L2_CAP_VBI_OUTPUT);

    case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
        return (cap->capabilities & V4L2_CAP_SLICED_VBI_CAPTURE);

    case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
        return (cap->capabilities & V4L2_CAP_SLICED_VBI_OUTPUT);

    case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
        return !exclude_overlay && (cap->capabilities & V4L2_CAP_VIDEO_OUTPUT_OVERLAY);

    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        return (cap->capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE);

    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        return (cap->capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE);
    }

    return 0;
}

static void device_formats(struct mg_connection *con,
                           char *device_name)
{
    struct v4l2_capability cap;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsize;
    char format[1024] = {'\0'};
    char formats[4096] = {'\0'};
    char result[65536] = {'\0'};
    int format_count = 0;
    int buffers_count = 0;
    int c;

    int fd = device_open(device_name);

    if (!fd)
    {
        mg_http_reply(con, 400, "", "Device can't be opened.");
        return;
    }

    memset(&cap, 0, sizeof(struct v4l2_capability));

    strcat(result, "{ ");
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) > -1)
    {

        memset(&fmtdesc, 0, sizeof(struct v4l2_fmtdesc));
        memset(&frmsize, 0, sizeof(struct v4l2_frmsizeenum));

        for (c = 1; c < 14; c++)
        {
            if (!device_buffer_check(&cap, c, 0))
            {
                continue;
            }

            fmtdesc.type = c;

            while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
            {
                fmtdesc.index++;
                frmsize.pixel_format = fmtdesc.pixelformat;
                frmsize.index = 0;
                while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0)
                {
                    format[0] = '\0';
                    if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
                    {
                        sprintf(format, FORMAT_DISCRETE,
                                pixfmtstr(fmtdesc.pixelformat),
                                frmsize.discrete.width,
                                frmsize.discrete.height);
                    }
                    else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
                    {
                        sprintf(format, FORMAT_STEPWISE,
                                pixfmtstr(fmtdesc.pixelformat),
                                frmsize.stepwise.min_width,
                                frmsize.stepwise.min_height,
                                frmsize.stepwise.max_width,
                                frmsize.stepwise.max_height,
                                frmsize.stepwise.step_width,
                                frmsize.stepwise.step_height);
                    }

                    if (format_count)
                    {
                        strcat(formats, ", ");
                    }
                    strcat(formats, format);
                    format_count++;
                    frmsize.index++;
                }
            }
            if (buffers_count)
            {
                strcat(result, ", ");
            }
            strcat(result, "\"");
            strcat(result, v4l2_buffer_type_names[c - 1].name);
            strcat(result, "\": { ");
            strcat(result, formats);
            strcat(result, " }");
            buffers_count++;
        }
    }
    strcat(result, " }\n");
    close(fd);

    mg_http_reply(con, 200, "Content-Type: application/json\r\n", result);
}

static char *field_name_get(int field)
{
    char *snum;
    if (field > -1 && field < 10)
    {
        return strdup(v4l2_field_names[field].name);
    }
    snum = malloc(10);
    sprintf(snum, "%d", field);
    return snum;
}

static char *colorspace_name_get(int colorspace)
{
    char *snum;
    if (colorspace > -1 && colorspace < 13)
    {
        return strdup(v4l2_colorspace_names[colorspace].name);
    }
    snum = malloc(10);
    sprintf(snum, "%d", colorspace);
    return snum;
}

static void device_format_get(struct mg_connection *con,
                              char *device_name)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    char status[1024] = {'\0'};
    char pix_format[1024] = {'\0'};
    char result[65536] = {'\0'};
    char *field_name;
    char *colorspace_name;
    int buffers_count = 0;
    int c;

    int fd = device_open(device_name);

    if (!fd)
    {
        mg_http_reply(con, 400, "", "Device can't be opened.");
        return;
    }

    memset(&cap, 0, sizeof(struct v4l2_capability));

    strcat(result, "{ ");

    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) > -1)
    {
        for (c = 1; c < 14; c++)
        {
            if (!device_buffer_check(&cap, c, 1))
            {
                continue;
            }

            memset(&fmt, 0, sizeof(struct v4l2_format));

            pix_format[0] = '\0';
            fmt.type = c;

            if (buffers_count)
            {
                strcat(result, ", ");
            }
            strcat(result, "\"");
            strcat(result, v4l2_buffer_type_names[c - 1].name);
            strcat(result, "\": { ");

            if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
            {
                field_name = field_name_get(fmt.fmt.pix.field);
                colorspace_name = colorspace_name_get(fmt.fmt.pix.colorspace);

                sprintf(pix_format, FORMAT_PIX_FORMAT,
                        fmt.fmt.pix.width,
                        fmt.fmt.pix.height,
                        pixfmtstr(fmt.fmt.pix.pixelformat),
                        field_name,
                        fmt.fmt.pix.bytesperline,
                        fmt.fmt.pix.sizeimage,
                        colorspace_name,
                        fmt.fmt.pix.priv,
                        fmt.fmt.pix.flags);

                free(field_name);
                free(colorspace_name);

                strcat(result, pix_format);
            }
            else
            {
                sprintf(status, "\"status\": \"%s\"", strerror(errno));
                strcat(result, status);
            }
            strcat(result, " }");

            buffers_count++;
        }
    }

    strcat(result, " }\n");
    mg_http_reply(con, 200, "Content-Type: application/json\r\n", result);
}

static int check_request(
    struct mg_connection *c,
    struct mg_http_message *hm,
    char *url,
    int enabled_methods,
    char *device_name)
{

    int url_length = strlen(url) - 1;

    if (hm->uri.len - url_length < 128)
    {
        memcpy(device_name, hm->uri.ptr + url_length, hm->uri.len - url_length);
    }
    else
    {
        mg_http_reply(c, 400, "", "Device name too long.");
        return -1;
    }

    if ((enabled_methods & METHOD_GET) && !strncmp(hm->method.ptr, "GET", 3))
    {
        return METHOD_GET;
    }

    if ((enabled_methods & METHOD_POST) && !strncmp(hm->method.ptr, "POST", 4))
    {
        return METHOD_POST;
    }

    mg_http_reply(c, 405, "", "Unsupported method.");

    return -1;
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    char *method = malloc(128);
    char *device_name = malloc(128);
    char *peer = malloc(128);

    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        mg_ntoa(&c->peer, peer, 128);

        LOGINFO("%s %.*s %.*s (%lu bytes)",
                peer,
                (int)hm->method.len, hm->method.ptr,
                (int)hm->uri.len, hm->uri.ptr,
                (unsigned long)hm->body.len);

        if (mg_http_match_uri(hm, URL_DEVICES))
        {
            device_list(c);
        }
        else if (mg_http_match_uri(hm, URL_DEVICE_FORMATS))
        {
            switch (check_request(c, hm, URL_DEVICE_FORMATS, METHOD_GET, device_name))
            {
            case METHOD_GET:
                device_formats(c, device_name);
                break;

            default:
                break;
            };
        }
        else if (mg_http_match_uri(hm, URL_DEVICE_CONTROL))
        {
            switch (check_request(c, hm, URL_DEVICE_CONTROL, METHOD_GET | METHOD_POST, device_name))
            {
            case METHOD_GET:
                device_control_get(c, device_name);
                break;

            case METHOD_POST:
                device_control_set(c, hm, device_name);
                break;

            default:
                break;
            };
        }
        else if (mg_http_match_uri(hm, URL_DEVICE_FORMAT))
        {
            switch (check_request(c, hm, URL_DEVICE_FORMAT, METHOD_GET, device_name))
            {
            case METHOD_GET:
                device_format_get(c, device_name);
                break;

            default:
                break;
            };
        }
        else
        {
            mg_http_reply(c, 404, "", "");
        }
    }
    free(method);
    free(device_name);
    (void)fn_data;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Available options are\n");
    fprintf(stderr, " -d            Enable debug log messages\n");
    fprintf(stderr, " -h            Print this help screen and exit\n");
    fprintf(stderr, " -i address    IP address for listening\n");
    fprintf(stderr, " -p port       Port for listening (number between 80 and 65535)\n");
}

int main(int argc, char *argv[])
{
    int opt;
    struct mg_mgr mgr;

    while ((opt = getopt(argc, argv, "dhi:p:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            debug_enabled = 1;
            break;

        case 'h':
            usage(argv[0]);
            return 1;

        case 'i':
            listen_ip = optarg;
            break;

        case 'p':
            if (digits_only(optarg) && atoi(optarg) > 79 && atoi(optarg) < 65536)
            {
                listen_port = optarg;
            }
            else
            {
                printf("ERROR: Invalid port number '%s'\n", optarg);
                return 1;
            }
            break;

        default:
            printf("ERROR: Invalid option '-%c'\n", opt);
            return 1;
        }
    }

    strcat(s_listen_on, "http://");
    strcat(s_listen_on, listen_ip);
    strcat(s_listen_on, ":");
    strcat(s_listen_on, listen_port);

    LOGINFO("Starting video-control-rest");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, s_listen_on, fn, NULL);

    LOGINFO("Listen on %s", s_listen_on);

    while (s_signo == 0)
    {
        mg_mgr_poll(&mgr, 100);
    }
    mg_mgr_free(&mgr);

    LOGINFO("Exiting on signal %d", s_signo);

    return 0;
}
