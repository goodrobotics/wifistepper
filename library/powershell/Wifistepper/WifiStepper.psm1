﻿## TODOs:
## O Enable Credencial REST methods
## O Enable Daisy Chaining


function Get-StepperPos {
<#
.SYNOPSIS
    This functions gets the current postion of the Stepper motor from the WIFI Stepper Controller

.DESCRIPTION
    Get-StepperPos is a function that returns the current postion of the Stepper Motor. The specified
    controller returns the postion, in steps (or MicroSteps) from the zero position. This will be a 
    22-bit signed integer ranging from (-2^21 to +2^21 - 1) and will overflow/underflow without errors.

.PARAMETER Stepper
    The remote stepper controller to check the position on.

.EXAMPLE
     Get-StepperPos -Stepper 172.17.17.2

.EXAMPLE
     Get-StepperPos -Stepper wx100.local

.INPUTS
    String

.OUTPUTS
    Interger

.NOTES
    Author:  Jason Kowalczyk
.LINK 
    https://wifistepper.com
#>

   [CmdletBinding()]
    Param (
        [Parameter(Mandatory)]
        [string]$Stepper
)   
    $stepperSTATUS = Invoke-RestMethod -URI  "http://$stepper/api/motor/state"
    Return $stepperSTATUS.pos
} # end Function get-stepperPos

function Move-StepperError {
<#
.SYNOPSIS
    This functions puts the Stepper motor into a moving visible Error mode using the WIFI Stepper Controller

.DESCRIPTION
    Move-StepperError is a function that moves the current postion of the Stepper Motor forward and back to 
    show an error visiblily in the project. It first does an Emergancy SOFT stop and the moves forward and backward
    a specified amount. This function will not return to the original workflow.

.PARAMETER Stepper
    The remote stepper controller to put into an Error mode.

.PARAMETER Move
    The amount to move forward and backward each 2 seconds. The Default Value is 300 Steps, at the current MicroStep size.

.EXAMPLE
     Move-StepperError -Stepper 172.17.17.2 

.EXAMPLE
     Move-StepperError -Stepper wx100.local -Move 20

.INPUTS
    String, Interger

.OUTPUTS
    None

.NOTES
    Author:  Jason Kowalczyk
.LINK 
    https://wifistepper.com
#>
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory)]
        [string]$Stepper,
        [ValidateNotNullOrEmpty()]
        [int]$Move = 300

)   
    # Emergency Stop
    $STOP_HIZ="true"
    $STOP_SOFT="true"
    Invoke-RestMethod -URI  "http://$stepper/api/motor/estop?hiz=$STOP_HIZ&soft=$STOP_SOFT" | out-null
    $stepperSTATUS = Invoke-RestMethod -URI  "http://$stepper/api/motor/state"
    while ($true) {
        $positionEMER = $stepperSTATUS.pos
        Invoke-RestMethod -URI  "http://$stepper/api/motor/goto?pos=$($positionEMER+$Move)" | out-null
        Start-Sleep -seconds 2
        Invoke-RestMethod -URI  "http://$stepper/api/motor/goto?pos=$positionEMER" | out-null
    }
    
} #end function move-steppererror

function Test-StepperComms {
<#
.SYNOPSIS
    This functions tests the communication to the WIFI Stepper Controller

.DESCRIPTION
    Test-StepperComms is a function that tests communication to the wifi stepper controllers. The specified
    controller returns a pong after receiving a ping. The function returns a True or False status.

.PARAMETER Stepper
    The remote stepper controller to test communication on.

.EXAMPLE
     Test-StepperComms -Stepper 172.17.17.2

.EXAMPLE
     Test-StepperComms -Stepper wx100.local

.INPUTS
    String

.OUTPUTS
    Boolean

.NOTES
    Author:  Jason Kowalczyk
.LINK 
    https://wifistepper.com
#>
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory)]
        [string]$Stepper
)   
    $stepperSTATUS = Invoke-RestMethod -URI  "http://$Stepper/api/ping" 
    if ($stepperSTATUS.data -eq "pong") { return $true } else { return $false }
} # end function Test-SetpperComms

function Write-StepperConfig {
## TODO Add in Default setup with a single -Default Switch?

<#
.SYNOPSIS
    This functions resets the Stepper motor using the Gountil command. It resets with switch input get drawn to low.

.DESCRIPTION
    Reset-StepperPostion resest the stepper zero position to a point when a sensor or switch draws the SW pin on the WiFi controller
    to a low voltage (ground). You can pass the function a speed in Steps per second to specifiy the speed the motor rotates to detect
    a sensor input. The function with Timeout by default in 120 seconds, which is able to be overidden on the commandline.

.PARAMETER Stepper
    The remote stepper controller to control.

.PARAMETER TimeOut
    The amount wait until timing out and entering an error mode. The default is 120 seconds.

.PARAMETER Mode
    Specifies which mode to operate the motor under. Current Mode utilizes the current sense 
    resistors to detect and maintain peak current. Under Voltage Mode, current sense resistors 
    are not necessary and may be bypassed by shorting the jumpers on the bottom side. Options 
    are "Current" or Voltage. 
    FACTORY DEFAULT: current
    
.PARAMETER StepSize
    StepSize - Utilize microstep waveforms. Under Current Mode 1 - 16 microstep is available. 
    With Voltage Mode down to 128 is available. Range available 1,2,4,8,16[,32,64,128] 
    FACTORY DEFAULT: 16 MicroSteps
    
.PARAMETER Ocd
    Over Current Detection Threshold - The voltage level over the commanded voltage (measured 
    at the driver MOSFETS) is considered an over voltage event. Range Available 0 - 1000 mV.
    FACTORY DEFAULT: 500 mV


.PARAMETER OcdShutdown
    Over Current Shutdown Enabled - When enabled, an Over Current flag (triggered by the Over 
    Current Detection Threshold) with shut down the bridges to prevent damage. 
    FACTORY DEFAULT: TRUE

.PARAMETER MaxSpeed
    Maximum Speed - Limit the motor shaft to this maximum speed. Any motion command over this 
    speed will be limited to this speed. Range Available 15.25 - 15610 steps/s 
    FACTORY DEFAULT: 10000 Steps/s

.PARAMETER MinSpeed
    Minimum Speed - Limit the minimum motor shaft to this speed. Any motion command under this 
    speed will be forced up to this speed. Range Available 0 - 976.3 steps/s
    FACTORY DEFAULT: 0 Steps/s
    
.PARAMETER Accel
    Acceleration - Defines how much acceleration to speed up the motor when commanding 
    higher speeds. Range Available 14.55 - 59590 steps/s^2
    FACTORY DEFAULT: 1000 Steps/s^2

    
.PARAMETER Decel
    Deceleration - Defines how much deceleration to slow down the motor when commanding 
    higher speeds. Range Available 14.55 - 59590 steps/s^2
    FACTORY DEFAULT: 1000 Steps/s^2
    
.PARAMETER FsSpeed
    Full Step Speed Changeover - Defines at which speed to change over from microstepping 
    to full steps. If set too low, there will be a noticeable jerk during transition. 
    Range available 7.63 - 15625 steps/s    
    FACTORY DEFAULT: 2000 Steps/s

.PARAMETER FsBoost
    Full Step Boost Enabled - When set, the amplitude of a full step is the peak of voltage 
    sinewave during microstepping, improving output current and motor torque. However, a 
    discontinuous jump in current is likely.
    FACTORY DEFAULT: FALSE

.PARAMETER Reverse
    Reverse Wires Enabled - Depending on how the wires are connected, the motor may rotate 
    backwards to what is commanded. When enabled, the wires are ‘virtually’ reversed through 
    software. Alternatively to this setting, you may reverse polarity of one of the A or B 
    motor wire connections.
    FACTORY DEFAULT: FALSE

.PARAMETER Save
    EEPROM Save - Save the configuration to the EEPROM. Upon power cycle, the configuration 
    state will be applied. Note: This is not a configuration entry. This is a directive to 
    write the active configuration to EEPROM once the configuration has been applied.
    FACTORY DEFAULT: N/A
    
.PARAMETER CMktHOLD
    KT Hold (Current Mode) - The peak current to drive the stepper motor when commanded to 
    hold a position (with no rotation). While this setting accepts values up to 14 amps, 
    it is recommended not to exceed 7 amps as holding a shaft position can cause current 
    DC states. Range Available 0.0 - 14.0 Amps.
    FACTORY DEFAULT: 2.5 Amps

.PARAMETER CMktRUN
    KT Run (Current Mode) - The peak current to drive the stepper motor when commanded to 
    rotate at a constant speed (no acceleration or deceleration). Range Available 0.0 - 14.0 Amps.
    FACTORY DEFAULT: 2.5 Amps

.PARAMETER CMktACCEL
    KT Acceleration (Current Mode) - The peak current to drive the stepper motor when commanded 
    to accelerate rotational speed. Range Available 0.0 - 14.0 Amps.
    FACTORY DEFAULT: 2.5 Amps
    
.PARAMETER CMktDECEL
    KT Deceleration (Current Mode) - The peak current to drive the stepper motor when commanded 
    to decelerate rotational speed. Range Available 0.0 - 14.0 Amps.
    FACTORY DEFAULT: 2.5 Amps
    
.PARAMETER CMSwitchperiod
    Switch Period (Current Mode) - The switching period of the current control algorithm. Range 
    Available 4.0-124.0 microseconds.
    FACTORY DEFAULT: 44 microseconds
    
.PARAMETER CMPredict
    Current Prediction Compensation Enabled (Current Mode) - When enabled, the current control 
    algorithm remembers offset adjustments and attempts to predict and minimize them in the future.
    FACTORY DEFAULT: TRUE

.PARAMETER CMMinOn
    Minimum On Time (Current Mode) - The minimum gate on time of the current control algorithm. 
    This setting impacts how the waveform is generated to control the current flowing through the 
    motor coils. Range Available 0.5 - 64.0 microseconds.
    FACTORY DEFAULT: 21 microseconds

.PARAMETER CMMinOff
    Minimum Off Time (Current Mode) - The minimum gate off time of the current control algorithm. 
    This setting impacts how the waveform is generated to control the current flowing through the 
    motor coils. Range Available 0.5 - 64.0 microseconds.
    FACTORY DEFAULT: 21 microseconds
    
.PARAMETER CMFastOff
    Fast Decay Time (Current Mode) - The maximum fast decay time of the current control algorithm. 
    This setting impacts how the waveform is generated to control the current flowing through the 
    motor coils. Range Available 2.0 - 32.0 microseconds.
    FACTORY DEFAULT: 4 microseconds

.PARAMETER CMFastStep
    Fast Decay Step Time (Current Mode) - The maximum fall step time of the current control 
    algorithm. This setting impacts how the waveform is generated to control the current flowing 
    through the motor coils. Range Available 2.0 - 32.0 microseconds.
    FACTORY DEFAULT: 20 microseconds
    
.PARAMETER VMktHOLD
    KT Hold (Voltage Mode) - The percentage of input voltage to drive the stepper motor when 
    commanded to hold a position (with no rotation). Range available 0.0-100.0 % of Vin.
    FACTORY DEFAULT: 15 % of Vin
    
.PARAMETER VMktRUN
    KT Run (Voltage Mode) - The percentage of input voltage to drive the stepper motor when 
    commanded to rotate at a constant speed (with no acceleration or deceleration). Range available
    0.0-100.0 % of Vin.
    FACTORY DEFAULT: 15 % of Vin

.PARAMETER VMktACCEL
    KT Acceleration (Voltage Mode) - The percentage of input voltage to drive the stepper motor 
    when commanded to accelerate rotational speed. Range available 0.0-100.0 % of Vin.
    FACTORY DEFAULT: 15 % of Vin

.PARAMETER VMktDECEL
    KT Deceleration (Voltage Mode) - The percentage of input voltage to drive the stepper motor 
    when commanded to decelerate rotational speed. Range available 0.0-100.0 % of Vin.
    FACTORY DEFAULT: 15 % of Vin
    
.PARAMETER VMPWMFreq
    PWM Frequency (Voltage Mode) - The PWM Frequency to drive the motor at when in Voltage 
    Control mode. Range available 2.8 - 62.5 kHz.
    FACTORY DEFAULT: 23.4 kHz
    
.PARAMETER VMStall
    Stall Threshold (Voltage Mode) - What voltage over the commanded voltage (measured at 
    the driver MOSFETS) is considered a motor stall. During a stall, the back EMF can be detected 
    and the stall flag asserted. Range availible 31.25 - 1000 mV
    FACTORY DEFAULT: 750 mV

.PARAMETER VMVoltComp
    Voltage Supply Compensation Enabled (Voltage Mode) - Utilize the power supply voltage 
    compensation feature. Certain unmanaged or less expensive power supplies will sag or 
    swell under current transients. To prevent changes in torque if this happens, the 
    Vin monitoring of the stepper motor driver can be utilized to increase or decrease the 
    KT vals during transients. Note: this will disable Vin input voltage measurement. 
    ** Important Note:** this requires board modification of the JP4 and R1 pads.
    FACTORY DEFAULT: FALSE

.PARAMETER VMBackEMFSlopeL
    Back EMF Low Slope Compensation (Voltage Mode) - How much to adjust the voltage to 
    compensate for Back EMF during slow speed operation on acceleration and deceleration 
    phases. Range available 0.0 - 0.4 %.
    FACTORY DEFAULT: 0.0375 %

.PARAMETER VMBackEMFSpeed
    Back EMF Slope Changeover Speed (Voltage Mode) - At which speed to change over between 
    slow and fast speed Back EMF slope compensation. Range available 0.0 - 976.5 Steps/s.
    FACTORY DEFAULT: 61.5072 Steps/s

.PARAMETER VMBackEMFSlopeHAccel
    Back EMF High Slope Acceleration Compensation (Voltage Mode) - How much to adjust the 
    voltage to compensate for Back EMF during fast speed operation on acceleration phase.
    Range available 0.0 - 0.4 %.
    FACTORY DEFAULT: 0.0615 %
    
.PARAMETER VMBackEMFSlopeHDeccel
    Back EMF High Slope Deceleration Compensation (Voltage Mode) - How much to adjust the 
    voltage to compensate for Back EMF during fast speed operation on deceleration phase.
    Range available 0.0 - 0.4 %.
    FACTORY DEFAULT: 0.0615 %

.EXAMPLE
     Write-StepperConfig -Stepper $WIFISTEPPER -mode current -StepSize 16 -Ocd 500 -OcdShutdown true -MaxSpeed 2000 -Accel 50 -Decel 50 

.EXAMPLE
     Write-StepperConfig -Stepper $WIFISTEPPER -mode voltage -StepSize 128 -Ocd 500 -OcdShutdown true -MaxSpeed 2000 -Accel 50 -Decel 50 

.INPUTS
    String, Interger, Boolean, Float

.OUTPUTS
    None

.NOTES
    Author:  Jason Kowalczyk
.LINK 
    https://wifistepper.com
#>
# Write motor configuration
    [CmdletBinding(SupportsShouldProcess)]
    param (
        [Parameter(Mandatory)]
        [string]$Stepper,
        [ValidateNotNullOrEmpty()]
        [int]$Timeout = 120,
        [ValidateSet("current","voltage")]
        [string[]]$Mode,
        [ValidateSet("1","2","4","8","16","32","64","128")]
        [int]$StepSize,
        [ValidateRange(0,1000)]
        [float]$Ocd = "500",
        [ValidateSet("true","false")]
        [string]$OcdShutdown = "true",
        [ValidateRange(15.25,15610)]
        [float]$MaxSpeed = $null,        
        [ValidateRange(0,976.3)]
        [float]$MinSpeed,    
        [ValidateRange(14.55,59590)]
        [float]$Accel,
        [ValidateRange(14.55,59590)]
        [float]$Decel,
        [ValidateRange(7.63,15625)]
        [float]$FsSpeed,
        [ValidateSet("true","false")]
        [string]$FsBoost,
        [ValidateSet("true","false")]
        [string]$Reverse,
        [ValidateSet("true","false")]
        [string]$Save,
        [ValidateRange(0,14)]
        [float]$CMktHOLD,
        [ValidateRange(0,14)]
        [float]$CMktRUN,
        [ValidateRange(0,14)]
        [float]$CMktACCEL,
        [ValidateRange(0,14)]
        [float]$CMktDECEL,
        [ValidateRange(4,124)]
        [float]$CMSwitchperiod,
        [ValidateSet("true","false")]
        [string]$CMPredict,
        [ValidateRange(.5,64)]
        [float]$CMMinOn,
        [ValidateRange(.5,64)]
        [float]$CMMinOff,
        [ValidateRange(2,32)]
        [float]$CMFastOff,
        [ValidateRange(2,32)]
        [float]$CMFastStep,
        [ValidateRange(0,100)]
        [float]$VMktHOLD,
        [ValidateRange(0,100)]
        [float]$VMktRUN,
        [ValidateRange(0,100)]
        [float]$VMktACCEL,
        [ValidateRange(0,100)]
        [float]$VMktDECEL,
        [ValidateRange(2.8,62.5)]
        [float]$VMPWMFreq,
        [ValidateRange(31.25,1000)]
        [float]$VMStall,
        [ValidateSet("true","false")]
        [string]$VMVoltComp,
        [ValidateRange(0,0.4)]
        [float]$VMBackEMFSlopeL,
        [ValidateRange(0,976.5)]
        [float]$VMBackEMFSpeed,
        [ValidateRange(0,0.4)]
        [float]$VMBackEMFSlopeHAccel,
        [ValidateRange(0,0.4)]
        [float]$VMBackEMFSlopeHDeccel
    )   

    BEGIN {
        # Init Variables
        [int]$i = 0
        ## Test Commnucation to Stepper
        Try {
            Test-StepperComms -Stepper $stepper -ErrorAction Stop | Out-Null
        } #end Try
        Catch {
            Write-Warning -Message "Can not Talk to Stepper - Comm check"
        }
        #create URI for REST request
        $BaseURI = "http://$Stepper/api/motor/set?"
        if ($PSBoundParameters.ContainsKey('Mode')) {   $BaseURI = $BaseURI + "mode=$Mode&"}
        if ($PSBoundParameters.ContainsKey('StepSize')) {   $BaseURI = $BaseURI + "stepsize=$StepSize&"}
        if ($PSBoundParameters.ContainsKey('Ocd')) {   $BaseURI = $BaseURI + "ocd=$Ocd&"}
        if ($PSBoundParameters.ContainsKey('OcdShutdown')) {   $BaseURI = $BaseURI + "ocdshutdown=$OcdShutdown&"}
        if ($PSBoundParameters.ContainsKey('MaxSpeed')) {   $BaseURI = $BaseURI + "maxspeed=$MaxSpeed&"}
        if ($PSBoundParameters.ContainsKey('MinSpeed')) {   $BaseURI = $BaseURI + "minspeed=$MinSpeed&"}
        if ($PSBoundParameters.ContainsKey('Accel')) {   $BaseURI = $BaseURI + "accel=$Accel&"}
        if ($PSBoundParameters.ContainsKey('Decel')) {   $BaseURI = $BaseURI + "decel=$Decel&"}
        if ($PSBoundParameters.ContainsKey('FsSpeed')) {   $BaseURI = $BaseURI + "fsspeed=$FsSpeed&"}
        if ($PSBoundParameters.ContainsKey('FsBoost')) {   $BaseURI = $BaseURI + "fsboot=$FsBoost&"}
        if ($PSBoundParameters.ContainsKey('CMktHOLD')) {   $BaseURI = $BaseURI + "cm_kthold=$CMktHOLD&"}
        if ($PSBoundParameters.ContainsKey('CMktRUN')) {   $BaseURI = $BaseURI + "cm_ktrun=$CMktRUN&"}
        if ($PSBoundParameters.ContainsKey('CMktACCEL')) {   $BaseURI = $BaseURI + "cm_ktaccel=$CMktACCEL&"}
        if ($PSBoundParameters.ContainsKey('CMktDECEL')) {   $BaseURI = $BaseURI + "cm_ktdecel=$CMktDECEL&"}
        if ($PSBoundParameters.ContainsKey('CMSwitchperiod')) {   $BaseURI = $BaseURI + "cm_switchperiod=$CMSwitchPeriod&"}
        if ($PSBoundParameters.ContainsKey('CMPredict')) {   $BaseURI = $BaseURI + "cm_predict=$CMPredict&"}
        if ($PSBoundParameters.ContainsKey('CMMinOn')) {   $BaseURI = $BaseURI + "cm_minon=$CMMinOn&"}
        if ($PSBoundParameters.ContainsKey('CMMinOff')) {   $BaseURI = $BaseURI + "cm_minoff=$CMMinOff&"}
        if ($PSBoundParameters.ContainsKey('CMFastOff')) {   $BaseURI = $BaseURI + "cm_fastoff=$CMFastOff&"}
        if ($PSBoundParameters.ContainsKey('CMFastStep')) {   $BaseURI = $BaseURI + "cm_fastoff=$CMFastStep&"}
        if ($PSBoundParameters.ContainsKey('VMktHOLD')) {   $BaseURI = $BaseURI + "vm_kthold=$VMktHOLD&"}
        if ($PSBoundParameters.ContainsKey('VMktRUN')) {   $BaseURI = $BaseURI + "vm_ktrun=$VMktRUN&"}
        if ($PSBoundParameters.ContainsKey('VMktACCEL')) {   $BaseURI = $BaseURI + "vm_ktaccel=$VMktAccel&"}
        if ($PSBoundParameters.ContainsKey('VMktDECEL')) {   $BaseURI = $BaseURI + "vm_ktdecel=$VMktDecel&"}
        if ($PSBoundParameters.ContainsKey('VMPWMFreq')) {   $BaseURI = $BaseURI + "vm_ktpwmfreq=$VMPWMFreq&"}
        if ($PSBoundParameters.ContainsKey('VMStall')) {   $BaseURI = $BaseURI + "vm_stall=$VMStall&"}
        if ($PSBoundParameters.ContainsKey('VMVoltComp')) {   $BaseURI = $BaseURI + "vm_volt_comp=$VMVoltComp&"}
        if ($PSBoundParameters.ContainsKey('VMBackEMFSlopeL')) {   $BaseURI = $BaseURI + "vm_bemf_slopel=$VMBackEMFSlopel&"}
        if ($PSBoundParameters.ContainsKey('VMBackEMFSpeed')) {   $BaseURI = $BaseURI + "vm_bemf_speedco=$VMBackEndSpeed&"}
        if ($PSBoundParameters.ContainsKey('VMBackEMFSlopeHAccel')) {   $BaseURI = $BaseURI + "vm_bemf_slopehacc=$VMBackEMFSlopeHAccel&"}
        if ($PSBoundParameters.ContainsKey('VMBackEMFSlopeHDeccel')) {   $BaseURI = $BaseURI + "vm_bemf_slopehdec=$VMBackEMFSlopeHDeccel&"}
        if ($PSBoundParameters.ContainsKey('Reverse')) {   $BaseURI = $BaseURI + "reverse=$Reverse&"}
        if ($PSBoundParameters.ContainsKey('Save')) {   $BaseURI = $BaseURI + "save=$Save&"}
        # Remove trailing &
        ##
        $URI = $BaseURI -replace '&$',''
    } ## END BEGIN
    PROCESS {
        ## Configure the Stepper Controller
        Try {
            if (($PSBoundParameters.Keys).count -gt "1") {
                    Invoke-RestMethod -URI  $URI | Out-Null
                } else {
                    Write-Warning "Can not configure Stepper no configuration Parameters given"
                }
        } #end Try
        Catch {
            Write-Warning -Message "Can write the Configutation to $Stepper"
        } #end Catch
    } #end PROCESS
} # End Write-StepperConfig

function Get-StepperConfig {
<#
.SYNOPSIS
    This functions reads the configuration from the WiFi Stepper Controller. It returns and 
	object of all the configuration parameters.

.DESCRIPTION
    Get-StepperConfig reads thc configutation and returns a PS object with all the variables. 

.PARAMETER Stepper
    The remote stepper controller to control.

.PARAMETER Mode
    Specifies which mode to operate the motor under. Current Mode utilizes the current sense 
    resistors to detect and maintain peak current. Under Voltage Mode, current sense resistors 
    are not necessary and may be bypassed by shorting the jumpers on the bottom side. Options 
    are "Current" or Voltage.
    
.PARAMETER StepSize
    StepSize - Utilize microstep waveforms. Under Current Mode 1 - 16 microstep is available. 
    With Voltage Mode down to 128 is available. Range available 1,2,4,8,16[,32,64,128]
    
.PARAMETER Ocd
    Over Current Detection Threshold - The voltage level over the commanded voltage (measured 
    at the driver MOSFETS) is considered an over voltage event. Range Available 0 - 1000 mV.

.PARAMETER OcdShutdown
    Over Current Shutdown Enabled - When enabled, an Over Current flag (triggered by the Over 
    Current Detection Threshold) with shut down the bridges to prevent damage.

.PARAMETER MaxSpeed
    Maximum Speed - Limit the motor shaft to this maximum speed. Any motion command over this 
    speed will be limited to this speed. Range Available 15.25 - 15610 steps/s

.PARAMETER MinSpeed
    Minimum Speed - Limit the minimum motor shaft to this speed. Any motion command under this 
    speed will be forced up to this speed. Range Available 0 - 976.3 steps/s
    
.PARAMETER Accel
    Acceleration - Defines how much acceleration to speed up the motor when commanding 
    higher speeds. Range Available 14.55 - 59590 steps/s^2
    
.PARAMETER Decel
    Deceleration - Defines how much deceleration to slow down the motor when commanding 
    higher speeds. Range Available 14.55 - 59590 steps/s^2
    
.PARAMETER FsSpeed
    Full Step Speed Changeover - Defines at which speed to change over from microstepping 
    to full steps. If set too low, there will be a noticeable jerk during transition. 
    Range available 7.63 - 15625 steps/s    

.PARAMETER FsBoost
    Full Step Boost Enabled - When set, the amplitude of a full step is the peak of voltage 
    sinewave during microstepping, improving output current and motor torque. However, a 
    discontinuous jump in current is likely.

.PARAMETER Reverse
    Reverse Wires Enabled - Depending on how the wires are connected, the motor may rotate 
    backwards to what is commanded. When enabled, the wires are ‘virtually’ reversed through 
    software. Alternatively to this setting, you may reverse polarity of one of the A or B 
    motor wire connections.

.PARAMETER Save
    EEPROM Save - Save the configuration to the EEPROM. Upon power cycle, the configuration 
    state will be applied. Note: This is not a configuration entry. This is a directive to 
    write the active configuration to EEPROM once the configuration has been applied.
    
.PARAMETER CMktHOLD
    KT Hold (Current Mode) - The peak current to drive the stepper motor when commanded to 
    hold a position (with no rotation). While this setting accepts values up to 14 amps, 
    it is recommended not to exceed 7 amps as holding a shaft position can cause current 
    DC states. Range Available 0.0 - 14.0 Amps.

.PARAMETER CMktRUN
    KT Run (Current Mode) - The peak current to drive the stepper motor when commanded to 
    rotate at a constant speed (no acceleration or deceleration). Range Available 0.0 - 14.0 Amps.

.PARAMETER CMktACCEL
    KT Acceleration (Current Mode) - The peak current to drive the stepper motor when commanded 
    to accelerate rotational speed. Range Available 0.0 - 14.0 Amps.
    
.PARAMETER CMktDECEL
    KT Deceleration (Current Mode) - The peak current to drive the stepper motor when commanded 
    to decelerate rotational speed. Range Available 0.0 - 14.0 Amps.
    
.PARAMETER CMSwitchperiod
    Switch Period (Current Mode) - The switching period of the current control algorithm. Range 
    Available 4.0-124.0 microseconds.
    
.PARAMETER CMPredict
    Current Prediction Compensation Enabled (Current Mode) - When enabled, the current control 
    algorithm remembers offset adjustments and attempts to predict and minimize them in the future.

.PARAMETER CMMinOn
    Minimum On Time (Current Mode) - The minimum gate on time of the current control algorithm. 
    This setting impacts how the waveform is generated to control the current flowing through the 
    motor coils. Range Available 0.5 - 64.0 microseconds.

.PARAMETER CMMinOff
    Minimum Off Time (Current Mode) - The minimum gate off time of the current control algorithm. 
    This setting impacts how the waveform is generated to control the current flowing through the 
    motor coils. Range Available 0.5 - 64.0 microseconds.
    
.PARAMETER CMFastOff
    Fast Decay Time (Current Mode) - The maximum fast decay time of the current control algorithm. 
    This setting impacts how the waveform is generated to control the current flowing through the 
    motor coils. Range Available 2.0 - 32.0 microseconds.

.PARAMETER CMFastStep
    Fast Decay Step Time (Current Mode) - The maximum fall step time of the current control 
    algorithm. This setting impacts how the waveform is generated to control the current flowing 
    through the motor coils. Range Available 2.0 - 32.0 microseconds.
    
.PARAMETER VMktHOLD
    KT Hold (Voltage Mode) - The percentage of input voltage to drive the stepper motor when 
    commanded to hold a position (with no rotation). Range available 0.0-100.0 % of Vin.
    
.PARAMETER VMktRUN
    KT Run (Voltage Mode) - The percentage of input voltage to drive the stepper motor when 
    commanded to rotate at a constant speed (with no acceleration or deceleration). Range available
    0.0-100.0 % of Vin.

.PARAMETER VMktACCEL
    KT Acceleration (Voltage Mode) - The percentage of input voltage to drive the stepper motor 
    when commanded to accelerate rotational speed. Range available 0.0-100.0 % of Vin.

.PARAMETER VMktDECEL
    KT Deceleration (Voltage Mode) - The percentage of input voltage to drive the stepper motor 
    when commanded to decelerate rotational speed. Range available 0.0-100.0 % of Vin.
    
.PARAMETER VMPWMFreq
    PWM Frequency (Voltage Mode) - The PWM Frequency to drive the motor at when in Voltage 
    Control mode. Range available 2.8 - 62.5 kHz.
    
.PARAMETER VMStall
    Stall Threshold (Voltage Mode) - What voltage over the commanded voltage (measured at 
    the driver MOSFETS) is considered a motor stall. During a stall, the back EMF can be detected 
    and the stall flag asserted.

.PARAMETER VMVoltComp
    Voltage Supply Compensation Enabled (Voltage Mode) - Utilize the power supply voltage 
    compensation feature. Certain unmanaged or less expensive power supplies will sag or 
    swell under current transients. To prevent changes in torque if this happens, the 
    Vin monitoring of the stepper motor driver can be utilized to increase or decrease the 
    KT vals during transients. Note: this will disable Vin input voltage measurement. 
    ** Important Note:** this requires board modification of the JP4 and R1 pads.

.PARAMETER VMBackEMFSlopeL
    Back EMF Low Slope Compensation (Voltage Mode) - How much to adjust the voltage to 
    compensate for Back EMF during slow speed operation on acceleration and deceleration 
    phases. Range available 0.0 - 0.4 %.

.PARAMETER VMBackEMFSpeed
    Back EMF Slope Changeover Speed (Voltage Mode) - At which speed to change over between 
    slow and fast speed Back EMF slope compensation. Range available 0.0 - 976.5 Steps/s.

.PARAMETER VMBackEMFSlopeHAccel
    Back EMF High Slope Acceleration Compensation (Voltage Mode) - How much to adjust the 
    voltage to compensate for Back EMF during fast speed operation on acceleration phase.
    Range available 0.0 - 0.4 %.
    
.PARAMETER VMBackEMFSlopeHDeccel
    Back EMF High Slope Deceleration Compensation (Voltage Mode) - How much to adjust the 
    voltage to compensate for Back EMF during fast speed operation on deceleration phase.
    Range available 0.0 - 0.4 %.

.EXAMPLE
     get-StepperConfig -Stepper $WIFISTEPPER 

.INPUTS
    String, Interger, Boolean, Float

.OUTPUTS
    None

.NOTES
    Author:  Jason Kowalczyk
.LINK https://wifistepper.com
#>
    [CmdletBinding(SupportsShouldProcess)]
    param (
        [Parameter(Mandatory)]
        [string]$Stepper
    )   

    BEGIN {
        # Init Variables
        $StepperConfig = ''
        ## Test Commnucation to Stepper
        Try {
            Test-StepperComms -Stepper $stepper -ErrorAction Stop | Out-Null
        } #end Try
        Catch {
            Write-Warning -Message "Can not Talk to Stepper - Comm check"
        }

        #create URI for REST request
        $URI = "http://$Stepper/api/motor/get"
		
    } ## END BEGIN
    PROCESS {
        ## Configure the Stepper Controller
        Try {
            $StepperConfig = Invoke-RestMethod -URI $URI 
        } #end Try
        Catch {
            Write-Warning -Message "Can read the Configutation to $Stepper"
        } #end Catch

        Return $StepperConfig
    } #end PROCESS

} # End Get-StepperConfig

function Reset-StepperPosition {
<#
.SYNOPSIS
    This functions resets the Stepper motor using the Gountil command. It resets with switch input get drawn to low.

.DESCRIPTION
    Reset-StepperPostion resest the stepper zero position to a point when a sensor or switch draws the SW pin on the WiFi controller
    to a low voltage (ground). You can pass the function a speed in Steps per second to specifiy the speed the motor rotates to detect
    a sensor input. The function with Timeout by default in 120 seconds, which is able to be overidden on the commandline.

.PARAMETER Stepper
    The remote stepper controller to control.

.PARAMETER ResetSpeed
    The amount to move forward in Steps per second.

.PARAMETER TimeOut
    The amount wait until timing out and entering an error mode. The default is 120 seconds.

.EXAMPLE
     Reset-StepperPostion -Stepper 172.17.17.2 

.EXAMPLE
     Reset-StepperPostion -Stepper wx100.local -ResetSpeed 20 -Timeout 60

.INPUTS
    String, Interger

.OUTPUTS
    None

.NOTES
    Author:  Jason Kowalczyk
.LINK 
    https://wifistepper.com
#>
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory)]
        [string]$Stepper,
        [Parameter(Mandatory)]
        [string]$ResetSpeed,
        [ValidateNotNullOrEmpty()]
        [int]$Timeout = 120

)   
    ## Reset Position Setting
    Start-Job -Name ResetPos -arg $Stepper,$ResetSpeed -ScriptBlock {
        param($Stepper,$ResetSpeed)
        # Stepper Movement Speed
        Invoke-RestMethod -URI  "http://$Stepper/api/motor/gountil?action=reset&dir=forward&stepss=$ResetSpeed" | Out-Null
        # Check if complete each half of second
        while ( (Invoke-RestMethod -URI  "http://$stepper/api/motor/state").movement -ne "idle") { Start-sleep -Milliseconds 250 } #end while
        return $true
    } #end Scriptblock
    # Wait for timeout if trigger is not hit 
    wait-job -name ResetPos -timeout $Timeout  
    receive-job -name ResetPos -EA Silentlycontinue
    Remove-Job -name ResetPos 

    if (Test-StepperBusy -Stepper $Stepper) {  
        ## ERROR MODE
        Move-StepperError -Stepper $Stepper
    }#endif Wait for Position reset
    
    #if Everything works move it back to Pos 0 (level and set this as the "top"
    Move-stepperMotor -Stepper $Stepper -Angle 0
} # end function RESET-stepperpostion

function Test-StepperBusy {
<#
.SYNOPSIS
    This functions tests if the WIFI Stepper Controller is BUSY moving to a position.

.DESCRIPTION
    Test-StepperBusy is a function that tests the wifi stepper controller to see if it is busy moving. The specified
    controller returns a True or False status.

.PARAMETER Stepper
    The remote stepper controller to test communication on.

.EXAMPLE
     Test-StepperBusy -Stepper 172.17.17.2

.EXAMPLE
     Test-StepperBusy -Stepper wx100.local

.INPUTS
    String

.OUTPUTS
    Boolean

.NOTES
    Author:  Jason Kowalczyk
.LINK 
    https://wifistepper.com
#>
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory)]
        [string]$Stepper
)   
    $stepperSTATUS = Invoke-RestMethod -URI  "http://$stepper/api/motor/state"
    if ($stepperSTATUS.movement -ne "idle") { return $true } else { return $false }
} # end function Tes-SetpperComms

function Move-StepperAngle {
<#
.SYNOPSIS
    This functions Stepper motor using the Goto command. It converts an angle to steps based on Microsteps setting configured in the controller.

.DESCRIPTION
    Move-StepperMotor moves the stepper to a new angle and calculates the proper amount of steps based on the MicroStep Setting. It will timeout
    and go into an error mode after a timeout.

.PARAMETER Stepper
    The remote stepper controller to control.

.PARAMETER Angle
    The new angle to move to, angles larger then 360 are allowed. The Angle is calculated based on the current MicroSteps setting in the Controller.

.PARAMETER TimeOut
    The amount wait until timing out and entering an error mode. The default is 120 seconds.

.EXAMPLE
     Move-StepperMotor -Stepper 172.17.17.2 -Angle 90

.EXAMPLE
     Move-StepperMotor -Stepper wx100.local -Angle 20 -Timeout 60

.INPUTS
    String, Interger

.OUTPUTS
    None

.NOTES
    Author:  Jason Kowalczyk
.LINK 
    https://wifistepper.com
#>

    [CmdletBinding(SupportsShouldProcess)]
    param (
        [Parameter(Mandatory)]
        [string]$Stepper,
        [Parameter(Mandatory)]
        [float]$Angle,
        [ValidateNotNullOrEmpty()]
        [int]$Timeout = 120
)   
    BEGIN {
        # Init Variables
        [int]$i = 0
        ## Test Commnucation to Stepper
        Try {
            Test-StepperComms -Stepper $stepper -ErrorAction Stop
        } #end Try
        Catch {
            Write-Warning -Message "Can not Talk to Stepper - Comm check"
        }
        ## Set StepSize
        Try {
            $MicroStepSize = $(Invoke-RestMethod -URI "http://$stepper/api/motor/get").stepsize
        } #end Try
        Catch {
            Write-Warning -Message "Can not Talk to Stepper - get config"
        }

        # Calculate Desired End Postion from Angle
        $pos = $Angle/360*200*$MicroStepSize


    } ## END BEGIN
    PROCESS {
        ## move the motor
        Try {
            # Move Motor to Specified Position
            Invoke-RestMethod -URI  "http://$Stepper/api/motor/goto?pos=$pos" | Out-null
            # Wait while stepper is Busy
            while ($(Test-StepperBusy -Stepper $Stepper)) { 
                Start-Sleep -Seconds 1
                $i++
                if( [int]$i -gt [int]$Timeout ) { Move-StepperError -Stepper $Stepper } #end if 
            } #end while
            #TODO Create Stepper Postion test (Test-StepperPosition function)
            #return True if stepper is at the correct location
            $stepperSTATUS = Invoke-RestMethod -URI  "http://$Stepper/api/motor/state"
            if (($stepperSTATUS.pos) -eq $pos ) { Return $true } else { Return $false }
        } #end Try
        Catch {
            Write-Warning -Message "Can not Move Stepper"
        } #end Catch
    } #end PROCESS

} # end Function Move-steppermotor

Export-ModuleMember -Function Get-StepperPos
Export-ModuleMember -Function Move-StepperError
Export-ModuleMember -Function Test-StepperComms
Export-ModuleMember -Function Write-StepperConfig
Export-ModuleMember -Function Get-StepperConfig
Export-ModuleMember -Function Reset-StepperPosition
Export-ModuleMember -Function Test-StepperBusy
Export-ModuleMember -Function Move-StepperAngle
#Export-ModuleMember -Function 
