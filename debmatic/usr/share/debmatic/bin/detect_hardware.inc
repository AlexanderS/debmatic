#!/bin/bash

if [ -e /etc/default/debmatic ]; then
  . /etc/default/debmatic
fi

for dev_no in {0..5}
do
  if [ $dev_no -eq 0 ]; then
    UART_DEV="raw-uart"
  else
    UART_DEV="raw-uart$dev_no"
  fi

  if [ -e "/dev/$UART_DEV" ]; then
    echo 1 > /sys/class/raw-uart/$UART_DEV/reset_radio_module

    HM_HMRF_ADDRESS=`/bin/eq3configcmd read-default-rf-address -f /dev/$UART_DEV -h 2>&1 | grep "^0x"`

    if [ -z "$HM_HMRF_ADDRESS" ] || [ "$HM_HMRF_ADDRESS" == "0x00ffff" ]; then
      RF_RPI_MOD_INFO=`timeout 20 /usr/bin/java -Dgnu.io.rxtx.SerialPorts=/dev/$UART_DEV -jar /opt/HmIP/hmip-copro-update.jar -p /dev/$UART_DEV -v -a 2>&1`
      HM_HMRF_ADDRESS=`echo "$RF_RPI_MOD_INFO" | sed -n 's/.*Radio address = \([0-9A-Fa-f]\{6\}\).*/0x\1/p'`
      HM_HMIP_SGTIN=`echo "$RF_RPI_MOD_INFO" | sed -n 's/.*SGTIN = \([0-9A-Fa-f]\{24\}\).*/\1/p'`

      if [ ! -z "$HM_HMRF_ADDRESS" ] || [ ! -z "$HM_HMIP_SGTIN" ]; then
        HM_HMRF_DEV='RPI-RF-MOD'
        HM_HMRF_SERIAL=`echo -n "$HM_HMIP_SGTIN" | tail -c 10`
        HM_HMRF_VERSION=`echo "$RF_RPI_MOD_INFO" | sed -n 's/.*Application version = \([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p'`
      fi
    else
      HM_HMRF_DEV='HM-MOD-RPI-PCB'
      HM_HMRF_SERIAL=`/bin/eq3configcmd update-coprocessor -p /dev/raw-uart -t HM-MOD-UART -c -se 2>&1 | grep "SerialNumber:" | cut -d' ' -f5`
      HM_HMRF_VERSION=`/bin/eq3configcmd update-coprocessor -p /dev/raw-uart -t HM-MOD-UART -c -v 2>&1 | grep "Version:" | cut -d' ' -f5`
      HM_HMIP_SGTIN=`/bin/eq3configcmd update-coprocessor -p /dev/raw-uart -t HM-MOD-UART -c -sg 2>&1 | grep "SGTIN:" | cut -d' ' -f5`
    fi

    if [ ! -z "$HM_HMRF_DEV" ]; then
      break;
    fi
  fi
done

if [ -z "$HM_HMRF_DEV" ]; then
  if [ -z "$DEBMATIC_SERIAL" ]; then
    DEBMATIC_SERIAL=`shuf -i 1-9999999 -n 1`
    DEBMATIC_SERIAL=`printf "DEB%07d" $DEBMATIC_SERIAL`
    echo "DEBMATIC_SERIAL=\"$DEBMATIC_SERIAL\"" >> /etc/default/debmatic
  fi

  if [ -z "$DEBMATIC_ADDRESS" ]; then
    DEBMATIC_ADDRESS=`shuf -i 1-16777215 -n 1`
    DEBMATIC_ADDRESS=`printf "0x%06x" $DEBMATIC_ADDRESS`
    echo "DEBMATIC_ADDRESS=\"$DEMATIC_ADDRESS\"" >> /etc/default/debmatic
  fi

  HM_HMRF_SERIAL=$DEBMATIC_SERIAL
  HM_HMRF_ADDRESS=$DEBMATIC_RADIO_MAC
else
  HM_HOST_RAW_UART="$UART_DEV"
  HM_HOST_GPIO_UART="/dev/$UART_DEV"
  HM_HMRF_DEVNODE="/dev/mmd_bidcos"
  HM_HMIP_DEV="$HM_HMRF_DEV"
  HM_HMIP_DEVNODE="/dev/mmd_hmip"
  HM_HMIP_ADDRESS="$HM_HMRF_ADDRESS"
  HM_HMIP_SERIAL="$HM_HMRF_SERIAL"
  HM_HMIP_VERSION="$HM_HMRF_VERSION"
fi

modprobe -a cp210x &> /dev/null

if [ `grep -c "1b1f c020" /sys/bus/usb-serial/drivers/cp210x/new_id` -eq 0 ]; then
  echo "1b1f c020" > /sys/bus/usb-serial/drivers/cp210x/new_id
fi

for syspath in $(find /sys/bus/usb/devices/usb*/ -name ttyUSB*); do
  if [ -e $syspath/dev ]; then
    eval "$(udevadm info -q property --export -p $syspath)"
    if [ "$ID_VENDOR_ID $ID_MODEL_ID" == "1b1f c020" ]; then
      HM_HMIP_DEV="HMIP-RFUSB"
      HM_HMIP_DEVNODE="$DEVNAME"
      HMIP_RFUSB_INFO=`timeout 20 /usr/bin/java -Dgnu.io.rxtx.SerialPorts=$HM_HMIP_DEVNODE -jar /opt/HmIP/hmip-copro-update.jar -p $HM_HMIP_DEVNODE -v -a 2>&1`
      HM_HMIP_ADDRESS=`echo "$HMIP_RFUSB_INFO" | sed -n 's/.*Radio address = \([0-9A-Fa-f]\{6\}\).*/0x\1/p'`
      HM_HMIP_SGTIN=`echo "$HMIP_RFUSB_INFO" | sed -n 's/.*SGTIN = \([0-9A-Fa-f]\{24\}\).*/\1/p'`
      HM_HMIP_SERIAL=`echo -n "$HM_HMIP_SGTIN" | tail -c 10`
      HM_HMIP_VERSION=`echo "$HMIP_RFUSB_INFO" | sed -n 's/.*Application version = \([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p'`
      break
    fi
  fi
done
