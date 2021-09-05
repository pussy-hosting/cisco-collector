# Simple IP Accounting

## cisco-collector

This sub-project aims to be an relatively independent collector from
the Cisco IOS `ip accounting` table via SNMP.

### Prerequisites

* A Cisco Router with IOS 12.5 or later (tested on a 2951 with 15.x)
* A working and accessible SNMP service on the router.
  A few thougts about SNMP:
```
snmp-server community XXXXXXXXXX RO                       <-- obviously change "public" to another "secret..."
snmp-server trap-source GigabitEthernet0/2.3              <-- if traps used, i wouldn't originate them from a public interface
snmp-server source-interface informs GigabitEthernet0/2.3 <-- also quering could be done from a separate interface
snmp-server location "some usefule information"
```
* Enabled IP accounting

  Example:

  We need to login. Just as a reminder :
```
enable
configure terminal
```
  **Please, do NOT copy and paste this configuration into your terminal.**
  
  Only the marked lines in the respecive scope are relevant, the rest is for informational use.
```
interface GigabitEthernet0/0
    description XXXXX_uplink
    ip address XX.XX.XX.78 255.255.255.252
    ip access-group 199 in
    ip accounting output-packets            <-- added to an outbound interface
!
interface GigabitEthernet0/1.6
    description public
    encapsulation dot1Q 6
    ip address XX.XX.XX.161 255.255.255.240
    ip accounting output-packets            <-- added to an inbound interface
!
ip accounting-threshold 16384             <-- hopefully this is enough for flushing every hour
ip accounting-list XX.XX.XX.160 0.0.0.15
ip accounting-list XX.XX.XX.76 0.0.0.3
ip accounting-list 10.0.0.0 0.255.255.255 <--- does also work for natted interfaces
ip accounting-transits 1                  <--- I don't have transits
!
kron policy-list hourly                   <--- The policy to run on occurance
    cli clear ip accounting                 <--- clear (aka transfer to checkpoint list)
!
kron occurrence hourly in 1:0 recurring   <--- Vaguely decided 1 Hour...
    policy-list hourly                      <--- The policy to run on occurance
```
Exit and out. Just as a reminder.
```
^Z
write
# copy running-config startup-config
```

* A working snmp agent.
  Example for Ubuntu 20.04 (and maybe for every Debian-like distro):
```
sudo apt install -y libsnmp-base snmp snmp-mibs-downloader
sudo download-mibs
sudo wget http://www.circitor.fr/Mibs/Mib/O/OLD-CISCO-IP-MIB.mib -O /usr/share/snmp/mibs/OLD-CISCO-IP-MIB.txt
sudo wget http://www.circitor.fr/Mibs/Mib/C/CISCO-SMI.mib -O /usr/share/snmp/mibs/CISCO-SMI.txt
# This fixes "Bad operator (INTEGER): At line 73 in /usr/share/snmp/mibs/ietf/SNMPv2-PDU"
sudo wget http://pastebin.com/raw.php?i=p3QyuXzZ -O /usr/share/snmp/mibs/ietf/SNMPv2-PDU
# Ugly but simple (one shouldn't do this), but as long as this is not fixed at IETF:
chattr +i /usr/share/snmp/mibs/ietf/SNMPv2-PDU
```
  Kudos to :
    * https://docs.linuxconsulting.mn.it/notes/net-snmp-errors-updated
    * http://www.circitor.fr/

* Again, a definition of your accountable (usually local) networks. refer to conf/localnet.cfg

## Building

*Work in Progress...*

## Running the collector

*Work in Progress*

## Caveats

This project probably doesn't scale well vertically, because
  a) The IP accounting table is kind of expensive.
  b) SNMP itself is painfully slow on most hardware.

## Tuning considerations

Routing (Switch Processing) capacity should be weight over the effort it takes to gather data.
To keep the IP accounting table in reasonable small size, we can trim down the recurring flushing to an interval of at least the time it takes for the SNMP task to get out the whole **checkpoint** table.
Indeed, to avoid loss of accountable traffic, we should always take data from the **checkpoint** table (`show ip accounting checkpoint` ) instead of the running table (`show ip accounting`).

To minimize the ip accounting  table furthermore, we could declare `ip accounting-list` definitions of our *accountable* CIDR's. Other traffic that doesn't originate or targets here, could be trimmed down to `ip accounting-transit` table-size. In my setup, this didn't change anything, as I previously only added `ip accounting output-packets` to the routed as well as to the natted interfaces in scope of accounting. To my knowledge `ip accounting`is not capable of aggregation based on SRC or DST IP's.

