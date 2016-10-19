gnome-terminal -e "bash -c \"./output/database PrimaryBackendOutput.log 6000; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./output/database SecondaryBackendOutput.log 6001; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./output/gateway ./ConfigFiles/Gateway_Config_P.txt PrimaryGatewayOutput.log 127.0.0.1 6000; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./output/gateway ./ConfigFiles/Gateway_Config_S.txt SecondaryGatewayOutput.log 127.0.0.1 6001; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./output/security ./ConfigFiles/SecurityConfig.txt SecurityDeviceOutput.log; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./output/door ./ConfigFiles/DoorConfig.txt InputFiles/DoorInput.txt DoorOutput.log; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./output/motion ./ConfigFiles/MotionConfig.txt InputFiles/MotionInput.txt MotionOutput.log; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./output/keychain ./ConfigFiles/KeychainConfig.txt InputFiles/KeychainInput.txt KeychainOutput.log; exec bash\""
