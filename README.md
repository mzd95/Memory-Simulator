# Memory-Simulator
ENEE 646 midterm project. Implementation of the Cortex-A53 memory system using a virtual memory simulator with a time scale to reveal the key steps such as instruction fetch, address generation and computation, tag searches in level 1and 2 caches, TLBs, page faults, and virtual to physical address translations.


For instruction fetches, a program counter (PC) should be used. It is incremented each count or it is updated by a branch instruction. The PC should be initialized to an address of a certain point of the program. A new pair of instructions are fetched for each new address in the PC. At random points in the time scale, the next instruction location should be randomly modified with the use of a branch instruction to generate instruction misses in various levels of the memory hierarchy. The simulation is carried out as a "virtual" fetch to avoid dealing with 32-bit physical addresses.


For operand fetches, up to 4 operands, each 64-bit long, may be loaded (stored) at a time from (into) L1 data cache into (from) a 32-register, 64-bit scratchpad over a 256-bit databus. Both the timing and reference addresses of load and store instructions should be randomized. As in instruction fetches, the actual process of loading or storing data in and out of the scratchpad registers need not be simulated. Cache hits and misses and page faults due to operand fetches are calculated.


The simulator is tested using “virtual programs”. Each instruction is represented by a 40-bit triple: (opcode, oprcount/bcondition, opraddress/brnaddress).


The cache search sequence is followed by the description in Hennessy's Computer Architecture.




LRU design:

The LRU structure is achieved by two arrays. Here the unified TLB is used as an example. Each block is associated with one example of a structure to store the TLB tag, page number corresponding to the TLB tag, and if there’s such data stored in the unit. Another array of the whole unified TLB unit is used to store the time stamp for recognition of the last used time of each block.


When a certain data is referred, its time stamp changes to the size of the TLB, and the time stamp of all other data with the same TLB tag decreases by 1.


When the data is not found in the TLB, it’ll be searched in the page table and added into TLB. If the TLB is full, the algorithm will choose the one with the minimum time stamp among the data with the same TLB tag, and replace it with the new data.
