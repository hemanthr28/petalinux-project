#! /bin/sh

################################################################################
# 250-SoC Daemon that checks and updates shared memory locations with PC Host
# in order to coordinate IP address used by ethernet-over-PCIe link.
# 
# Gordon McKillop <g.mckillop@nallatech.com>
# 2017-01-19
################################################################################

DELAY_PERIOD_SECONDS=15

# 250-SoC end point driver module name.
MODULE_NAME="nalla_250_soc"
KERNEL_REL=$(uname -r)
# TODO: Comment.
DRIVER_MODULE_LOCATION="/lib/modules/$KERNEL_REL/extra/nalla_250_soc.ko"

# Memory Address (from HPS perspective of shared memory location PC host will
# write the IP address the 250-SoC is supposed to use on ethernet-over-PCIe
# link.
PC_HOST_IP_INSTRUCTION_ADDRESS=0x80010800

# Memory Address (from HPS perspective of shared memory location SoC will write
# the IP address that the 250-SoC is currently using on ethernet-over-PCIe
# link.
SOC_IP_REPORT_ADDRESS=0x80011800

# The register has three bits to poll:
# Bit 0 – PCIe Reset (when ‘1’, the reset is active or has been active since the last time the register was read)
# Bit 1 – AXI Reset (when ‘1’, the reset is active or has been active since the last time the register was read)
# Bit 2 – BRAM Reset (when ‘1’, the reset is active or has been active since the last time the register was read)
PCIE_AXI_BRAM_RESET_ADDRESS=0x80021000

ETHERNET_ENABLE=1


################################################################################
#
################################################################################
#INPUT_VALUE=$(devmem $SOC_FIRMWARE_TYPE_ADDRESS)

#if [ "$INPUT_VALUE" != "$END_POINT_FIRMWARE_TYPE" ]
#then
#   echo "End point firmware not detected."
#   exit
#fi
#hwclock -s
################################################################################
#
################################################################################
while (true)
do
   RESET_STATE=$(devmem $PCIE_AXI_BRAM_RESET_ADDRESS)
   
   if [ $RESET_STATE = "0x00000000" ]
   then 
	# Check if driver is loaded.
	DRIVER_LOADED=$(lsmod | grep $MODULE_NAME | wc -l)

	# If the driver isn't load it, insert it.
	if [ $DRIVER_LOADED -eq 0 ]
	then
	   ### echo "Loading 250-SoC end point driver."

	   insmod $DRIVER_MODULE_LOCATION NetworkEnable=$ETHERNET_ENABLE

	   sleep 3

	   DRIVER_LOADED=1
	fi

	if [ $DRIVER_LOADED -ne 0 ] && [ $ETHERNET_ENABLE -eq 1 ]
	then
	   # Check that 250-SoC ethernet-over-PCIe interface has an IP address.
	   NETWORK_HAS_IP4_ADDRESS=$(ifconfig nalla0 | grep "inet " | wc -l)
	   NETWORK_UP=$(ifconfig nalla0 | grep "UP " | wc -l)

	   if [ $NETWORK_HAS_IP4_ADDRESS -ne 0 && $NETWORK_UP -ne 0 ]
	   then
		 # Get the current IP address 250-SoC ethernet-over-PCIe interface is
		 # using.
		 
		CURRENT_IP4_ADDRESS=$(ifconfig nalla0 | grep "inet "         \
						       | sed s/"addr:"/" "/g  \
						       | awk '{print $2}')

		 # Get the current IP4 address octets as space seperated decimals.
		 DECIMAL_OCTETS=$(echo $CURRENT_IP4_ADDRESS | sed s/"\."/" "/g)

		 # Convert the space seperated decimals into a 32bit hex string.
		 HEX_VALUE=$(printf "0x%02x%02x%02x%02x" $DECIMAL_OCTETS)

		 # Write the IP address 250-SoC has assumed to the shared location that
		 # PC host can reference.
		 devmem $SOC_IP_REPORT_ADDRESS w $HEX_VALUE
	   else
	     CURRENT_IP4_ADDRESS=0
		 # Write zero to the shared location that PC host can reference to
		 # indicate that we currently don't have an IP address.
		 devmem $SOC_IP_REPORT_ADDRESS w 0x0
	   fi

	   # Read the shared location that PC host can use to instruct 250-SoC which IP
	   # address to use.
	   INPUT_VALUE=$(devmem $PC_HOST_IP_INSTRUCTION_ADDRESS)

	   # Get individual bytes from 32 bit word read from shared memory.
	   TARGET_IP4_ADDRESS_BYTE1=$(((INPUT_VALUE ) & 0xFF))
	   TARGET_IP4_ADDRESS_BYTE2=$(((INPUT_VALUE >> 8) & 0xFF))
	   TARGET_IP4_ADDRESS_BYTE3=$(((INPUT_VALUE >> 16) & 0xFF))
	   TARGET_IP4_ADDRESS_BYTE4=$(((INPUT_VALUE >> 24) & 0xFF))

	   # Convert individual bytes into period seperated decimal octets that
	   # ipconfig can accept as a IP address.
	   TARGET_IP4_ADDRESS=$(printf "%d.%d.%d.%d" $TARGET_IP4_ADDRESS_BYTE4  \
							$TARGET_IP4_ADDRESS_BYTE3  \
							$TARGET_IP4_ADDRESS_BYTE2  \
							$TARGET_IP4_ADDRESS_BYTE1)

	   # If the shared location PC Host writes to wasn't blank, set the 250-SoC
	   # ethernet-over-PCIe interface to the instructed IP address.
	   if [ "$TARGET_IP4_ADDRESS" != "0.0.0.0" ]
	   then
		 if [ "$CURRENT_IP4_ADDRESS" != "$TARGET_IP4_ADDRESS" ]
		 then
		    ###echo "${CURRENT_IP4_ADDRESS} --> ${TARGET_IP4_ADDRESS}"

		    # Set 250-SoC ethernet-over-PCIe address to that intructed by PC
		    # host.
		    ifconfig nalla0 $TARGET_IP4_ADDRESS
		 fi
       else
			ifconfig nalla0 down
	   fi
	fi
   else
   	echo Waiting on resets. Register $PCIE_AXI_BRAM_RESET_ADDRESS: $RESET_STATE
   fi

   # Sleep for a period
   sleep $DELAY_PERIOD_SECONDS
done


# EOF - ep_daemon.sh
