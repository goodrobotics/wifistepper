---
layout: default
title: Command Queue
nav_order: 5
---
# Command Queue
The command queues consist of a number of seperate queues of verious sizes that commands can be placed and executed in an orderly fasion. Certain commands take time to execute and have have preconditions that need to be met or an error will occur.

**Queue 0** is known as the *Execution Queue* and is the only queue that executes commands on the motor. For a command to execute, it needs to be at the head of the *Execution Queue* and have the command-dependent preconditions met (eg. BUSY flag must not be asserted). At this point, the command is popped off the *Execution Queue* and executed on the motor. The next command becomes the new head and is evaluated, run, etc.

**Queue 1** is the initialization queue that is automatically executed on the power up sequence after motor configuration is loaded and applied.

Motor commands are always added to the end of a queue. Queue-management commands can empty, copy, and save/load queues and operate on the queue, but are not added to it. Saving a queue means it is serialized to a section of EEPROM on the device. On power cycle, all queues (excluding the *Execution Queue*) that were previously saved to EEPROM are loaded automatically. You must manually save queues (including *Queue 1*) prior to power cycle or the commands in the queue are lost.  

## Memory Layout
Commands in the queue allocate a specific amount of memory. Once the queue memory is exausted, attempting to add more commands to that queue will set an error flag. The queue memory sizes are defined below. 

![](/images/command-queue.png)

# Commands
There are three general categories of commands.

1. Settings and Configuration
2. Queue Management
3. Motor Control

**Settings and Configuration** commands are not added to the queue nor do they alter the queue. They are for things like access point connection, enabling services, and configuring interfaces and typically require rebooting the Wi-Fi Stepper once applied. They do not impact the command queue in any way and are not covered here. See page [Settings and Configuration](/commands/configuration.html) for more on these types of commands.

**Queue Management** commands act on entire queues as a unit. They are evaluated as soon as they are issued and can be used to save, load, copy queues, etc. For a full list see [Queue Management](/commands/queue.html) 

Finally, **Motor Control** commands usually act on the motor and are added to a queue and evaluated only when they are at the head of the *Execution Queue*. Typical motor commands that are added to a queue are *[Run](/commands/motor.html#run)*, *[GoTo](/commands/motor.html#goto)*, and *[SetConfig](/commands/motor.html#setconfig)*. An exhaustive list of these commands can be found at *[Motor Commands](/commands/motor.html)*. Special note is the *[RunQueue](/commands/motor.html#runqueue)* command that is enqueued and evaluated only when it is at the head of the *Execution Queue*. At this point, it expands the `targetqueue` into the head of the *Execution Queue*, moving existing enqueued commands down. The *RunQueue* command can be used to create recursive loops when added to the end of a (non-Execution) queue, and then the queue is coppied into the Execution Queue.
