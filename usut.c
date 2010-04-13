/*
 * User space USB testing implemented with libusb.
 * Copyright (C) 2010 Aldo Brett Cedillo Martinez <x0130339@ti.com>
 *
 * Based in testusb.c developed by David Brownell.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#include <string.h>

#include <libusb.h>

#define TEST_CASES		30

/* Defines used for bulk transfers with libusb */
#define TIMEOUT			500
#define BULK_IN			0x81	//In end point.
#define BULK_OUT		0x01 	//Out end point.

#define LOOPBACK_CONFIG		2	//Configuration numeration used in Gadget Zero.
#define SOURCESINK_CONFIG	3

/* FIXME pattern should be available for user to change, should this also be done 
 * using get opt?
 */
static unsigned pattern = 0;
static int deviceConfiguration = 0;

struct usbtest_param {	//This structure is used to make work as testusb.c
	//inputs
	unsigned		test_num;	/*0..(TEST_CASES-1)*/
	unsigned 		iterations;
	unsigned 		length;
	unsigned		vary;
	unsigned 		sglen;

	//outputs
//	struct timeval		duration;	//FIXME how to use this?
};

enum usb_device_speed {
	USB_SPEED_UNKNOWN = 0,		/* enumerating */
	USB_SPEED_LOW, USB_SPEED_FULL, 	/* usb 1.1 */
	USB_SPEED_HIGH			/* usb 2.0 */
};

struct testdev {
	struct testdev		*next;
	
	libusb_device_handle	*devHandle;	//Instead of the name of the proc
	//char			*name;		//entrance, I carry the device handle.

	//pthread_t		thread;	//FIXME in the original it has this but doesn't use it.
	enum usb_device_speed	speed;
	unsigned		ifnum:8;
	unsigned		forever:1;
	int 			test;

	struct usbtest_param	param;
};
static struct testdev *testdevs;

/* USB TESTS */
static int simple_io (  struct testdev  *dev,
                        int             iterations,
                        int             vary,
                        int             expected,
                        const char      *label,
                        char            direction       /* FIXME I think this term should be included in
                                                         * one of the structures, maybe testdev. 
                                                         */
                     );


static int usbtest_tests (struct testdev *dev)
{
	int retval = 0;
	
	switch (dev->param.test_num) {
		case 1:
			/* usbtest driver checks if there is an out_pipe, 
			 * check how to do this.
			 */
			printf ("TEST 1: write %d bytes %u times\n", dev->param.length, dev->param.iterations);
                        retval = simple_io (dev, dev->param.iterations, 0, 0, "Test 1", 0);
			break;
		case 2:
			/* usbtest driver checks if there is an in_pipe, 
			 * check how to do this.
			 */
			printf ("TEST 2: read %d bytes %u times\n", dev->param.length, dev->param.iterations);
			retval = simple_io (dev, dev->param.iterations, 0, 0, "Test 2", 1);
			break;
		case 3:
			printf ("TEST 3: write/%d 0..%d bytes %u times\n", dev->param.vary, dev->param.length,
				dev->param.iterations);
			retval = simple_io (dev, dev->param.iterations, dev->param.vary, 0, "Test 3", 0);
			break;
		case 4:
			printf ("TEST 4: read/%d 0..%d bytes %u times\n", dev->param.vary, dev->param.length,
				dev->param.iterations);
			retval = simple_io (dev, dev->param.iterations, dev->param.vary, 0, "Test 4", 1);
			break;
	}

	return retval;
}

static int is_testdev (libusb_device *dev)
{
	struct libusb_device_descriptor devDesc;

	if (libusb_get_device_descriptor (dev, &devDesc) < 0) {
		perror ("Cannot get descriptor\n");
		return 0;
	}

	/* FX2 with (tweaked) bulksrc firmware */
	if (devDesc.idVendor == 0x0547 && devDesc.idProduct == 0x1002)
		return 1;

	/*----------------------------------------------------*/

	/* devices that start up using the EZ-USB default device and
	 * which we can use after loading simple firmware.  hotplug
	 * can fxload it, and then run this test driver.
	 *
	 * we return false positives in two cases:
	 * - the device has a "real" driver (maybe usb-serial) that
	 *   renumerates.  the device should vanish quickly.
	 * - the device doesn't have the test firmware installed.
	 */

	/* generic EZ-USB FX controller */
	if (devDesc.idVendor == 0x0547 && devDesc.idProduct == 0x2235)
		return 1;

	/* generic EZ-USB FX2 controller */
	if (devDesc.idVendor == 0x04b4 && devDesc.idProduct == 0x8613)
		return 1;

	/* CY3671 development board with EZ-USB FX */
	if (devDesc.idVendor == 0x0547 && devDesc.idProduct == 0x0080)
		return 1;

	/* Keyspan 19Qi uses an21xx (original EZ-USB) */
	if (devDesc.idVendor == 0x06cd && devDesc.idProduct == 0x010b)
		return 1;

	/*----------------------------------------------------*/

	/* "gadget zero", Linux-USB test software */
	if (devDesc.idVendor == 0x0525 && devDesc.idProduct == 0xa4a0)
		return 1;

	/* user mode subset of that */
	if (devDesc.idVendor == 0x0525 && devDesc.idProduct == 0xa4a4)
		return 1;

	/* iso version of usermode code */
	if (devDesc.idVendor == 0x0525 && devDesc.idProduct == 0xa4a3)
		return 1;

	/* some GPL'd test firmware uses these IDs */

	if (devDesc.idVendor == 0xfff0 && devDesc.idProduct == 0xfff0)
		return 1;

	/*----------------------------------------------------*/

	/* iBOT2 high speed webcam */
	if (devDesc.idVendor == 0x0b62 && devDesc.idProduct == 0x0059)
		return 1;

	return 0;
}

static int find_testdev (void)
{
	int cnt, i, status;
	libusb_device **devs;

	cnt = libusb_get_device_list (NULL, &devs);
	if (cnt < 0) {
		perror ("Can't open libusb device liste\n");
		return -1;
	}

	for (i = 0; i <cnt; i++)
	{
		if (is_testdev (devs[i])) {
			struct testdev *entry;
			libusb_device_handle *devHandle;

			if (libusb_open (devs[i], &devHandle) < 0) {
				fprintf (stderr, "Couldn't get libusb_device_handle for device\n");
			}


                        status = libusb_kernel_driver_active (devHandle, 0);
                        if (status == 1)
                        {
                                fprintf (stderr, "Going to detach kernel driver\n");
                                status = libusb_detach_kernel_driver (devHandle, 0);
                                if (status != 0) {
                                        fprintf (stderr, "libusb_detach_kernel_driver = %d\n", status);
                                        return status;
                                }
                        }

			if (libusb_get_configuration (devHandle, &deviceConfiguration) < 0) {
				fprintf (stderr, "Couln't get libusb_get_configuration for device\n");
			}

			if ((entry = calloc (1, sizeof (*entry))) == 0) {
				fputs ("no mem!\n", stderr);
				goto done;
			}

			entry->devHandle = devHandle;
			if (!entry->devHandle) {
				free (entry);
				goto done;
			}
			
			//FIXME check how to find to which interface the device is bounded.
			entry->ifnum = 0;
			
			//FIXME check how to find out the speed of the device with libusb.
		
//			fprintf (stderr, "%s speed
			entry->next = testdevs;
			testdevs = entry;

			printf ("g_zero baby!\n");
		}
	}

done:
	libusb_free_device_list (devs, 1);

	return 0;
}

static void *handle_testdev (void *arg)
{
	struct testdev	*dev = arg;
	int 		i, status;

restart:
	for (i = 0; i < TEST_CASES; i++) {
		if (dev->test != -1 && dev->test != i)
			continue;

		dev->param.test_num = i;

		/* In original testusb usbdev_ioctl function is called which makes
		 * ioctl calls to usbtest drivers. Since this program uses "libusb"
		 * no ioctl calls are needed, in this case the ioctl cases will be
		 * implemented in this program. 
		 */
		status = usbtest_tests (dev);

		if (status < 0) {
			fprintf (stderr, "error in simple_io\n");
		}
		else
			printf ("Test %i :: OK\n", dev->param.test_num);
	}

	if (dev->forever)
		goto restart;

	return arg;	
}


int main(int argc, char **argv)
{
	int c;
	struct testdev 		*entry;
	char 			*device; //FIXME we won't use the devices procs name so maybe this variable won't exist.
	int 			all = 0, forever = 0, not = 0;
	int 			test = -1; /* -1 = all */
	struct usbtest_param 	param;

//	libusb_device_handle *devHandle;
//	libusb_device **devs;
	int status = 0;

	/* pick defaults that work with all speeds, without short packets. 
	 * FIXME don't understand this.
	 */
	param.iterations = 1000;
	param.length = 512;
	param.vary = 512;
	param.sglen = 32;
	param.test_num = 0;	//This initialization was done so compiler stopped sending a warning.

	/* for easy use when hotplugging, FIXME don't get it */
	device = getenv ("DEVICE");

	while ((c = getopt (argc, argv, "D:ac:g:hnps:t:v:")) != EOF)
	switch (c) {
		case 'a':	/* use all devices */
			device = 0;
			all = 1;
			continue;
		case 'c':	/* count iterations */
			param.iterations = atoi (optarg);
			if (param.iterations < 0)
				goto usage;
			continue;
		case 'l':	/* loop forever */
			forever = 1;
			continue;
		case 'n':	/* no test running */
			not = 1;
			continue;
		case 'p':
			pattern = 1;
			continue;
		case 's':	/* size of packets */
			param.length = atoi (optarg);
			if (param.length < 0)
				goto usage;
			continue;
		case 't':	/* run just one test */
			test = atoi (optarg);
			if (test < 0)
				goto usage;
			continue;
		case '?':
		case 'h':
		default:
usage:
                fprintf (stderr, "usage: %s [-an] [-D dev]\n"
                        "\t[-c iterations]  [-t testnum]\n"
                        "\t[-s packetsize] [-g sglen] [-v vary]\n",
                        argv [0]);
		
		return 1;
	}
	if (optind != argc)
		goto usage;

	if (!all && !device) {
		fprintf (stderr, "must specify '-a' or '-D dev'."
			"or DEVICE=/proc/bus/usb/BBB/DDD in env\n");
		goto usage;
	}

	if ((c = open("/proc/bus/usb/devices", O_RDONLY)) < 0) {
		fprintf (stderr, "usbfs files are missing\n");
		return -1;
	}
	
	status = libusb_init(NULL);
	if (status < 0)	{
		fprintf (stderr, "libusb couln't initialize due to error %d\n", status);
		return status;
	}

	/* Collect and list the devices */
	status = find_testdev ();
	if (status < 0) {
		fprintf (stderr, "find_testdev function failed\n");
		return status;
	}

	/* Quit, run single test, or create test threads */ //FIXME is it necessary to use threads?
	if (!testdevs && !device) {
		fprintf (stderr, "no test devices recognized\n");
		return -1;
	}

	if (not)
		return 0;

//	if (testdevs && testdevs->next == 0 && !device)	/* FIXME */
//		device = testdevs->name;

	for (entry = testdevs; entry; entry = entry->next) {
//		int status;

		entry->param = param;
		entry->forever = forever;
		entry->test = test;

		return handle_testdev (entry) != entry;
	}

	libusb_exit(NULL);

	printf ("Fin, %i\n", param.length);
	return 0;
}

static inline void simple_fill_buf (char  *buffer, unsigned len)
{
	unsigned i;

	switch (pattern) {
	default:
		//FALLTHROUGH
	case 0:
		memset (buffer, 0, len);
		break;
	case 1:	/* mod63 */
		for   (i = 0; i < len; i++)
			*buffer++ = (char) (i % 63);
		break;
	}
	
}

static inline int simple_check_buf (char *buffer, unsigned len)
{
	unsigned 	i;
	char 		expected;

	for (i = 0; i < len; i++, buffer++) {
		switch (pattern) {
		/* all zeroes has no synchronization issues */
		case 0:
			expected = 0;
			break;
		/* mod63 stays in sync with short-terminated transfers,
		 * or otherwise when host and gadget agree on how large
		 * each usb transfer request should be.  resync is done
		 * with set_interface or set_config.
		 */
		case 1:		/* mod63 */
			expected = i % 63;
			break;
		/* always fail unsupported patterns */
		default:
			expected = !*buffer;
			break;
		}

		if (*buffer == expected)
			continue;
		fprintf (stderr, "buf[%d] = %d (not %d)\n", i, *buffer, expected);
		return -1; //FIXME write adequate error (EINVAL)
	}

	return 0;
}

static int simple_io (  struct testdev  *dev,
                        int             iterations,
                        int             vary,
                        int             expected,
                        const char      *label,
			char 		direction	/* FIXME I think this term should be included in
							 * one of the structures, maybe testdev. 
							 */
		     )
{
	char *buf;
	int retval = 0;
	int transferred;
	int max = dev->param.length;

	buf = calloc (dev->param.length, sizeof(char));

	while (retval == 0 && iterations-- > 0) {
		if (direction == 0) {		//Out
			simple_fill_buf (buf, dev->param.length);

			retval = libusb_bulk_transfer (dev->devHandle, BULK_OUT, buf, dev->param.length, &transferred, TIMEOUT);
	                if (retval != 0) {
        	                fprintf (stderr, "Bulk write error %d\n", retval);
                	        break;
			}
                }
		else if (direction == 1) {	//In
			retval = libusb_bulk_transfer (dev->devHandle, BULK_IN, buf, dev->param.length, &transferred, TIMEOUT);
			if (retval != 0) {
				fprintf (stderr, "Bulk read error %d\n", retval);
				break;
			}

			if (retval == 0)
				retval = simple_check_buf (buf, dev->param.length);
		}

		if (vary) {		//FIXME implement this.
			int len = dev->param.length;

			len += vary;
			len %= max;

			if (len == 0)
				len = (vary < max) ? vary : max;
			dev->param.length = len;
		}

	}

	if (expected != retval)
		fprintf (stderr, "failed, %d iterations left\n", dev->param.length);

	return retval;
}
