# OsoLogic® - Open Industrial Automation Platform - CE

![OSOLOGIC logo](logos/osologic_logo.png)

**(C) Roig Borrell S.L. · Ibercomp S.L.**

## Project Names

- Borrell Automation Platform  
- Borrell PlantManager OS  
- OsoLogic OS  
- OsoLogic Hub  
- Ibercomp Altair OS  

An open-source hardware and software initiative to modernize industrial and home automation, bridging the gap between traditional PLC systems and the powerful, flexible world of modern computing.

## Overview

This project aims to create a fully open hardware and software platform that can serve both as an alternative to existing PLC and IoT systems and as a standard for the ecosystem of single-board computers and microcontroller-based platforms, including maker boards and DIY electronics.

We want to help industry leap forward by adopting modern, flexible technologies, freeing it from planned obsolescence and the limitations of proprietary, closed systems that still dominate machine control, factory automation, and smart environments.

## Why?

Most existing automation platforms are closed, rigid, and expensive. Modern computing offers better tools, better scalability, and better integration, but the gap remains between industrial-grade reliability and the flexibility of modern development tools.

We are building a bridge.

## Core Principles

- **Open and hackable** – Built from the ground up with open-source hardware and software.  
- **Real-time and Linux-based** – Optimized Linux distributions with real-time capabilities.  
- **Data-centric** – In-memory, real-time database at the core for flexible and immediate system interaction.  
- **Modular and compatible** – Interfaces with legacy industrial standards (IEC 61131-3, Ladder, ST) and supports modern technologies.  
- **Secure by design** – Encryption, certificates, authentication, firewalls, and more.  
- **Universal gateway** – Communicates across industrial protocols (Modbus, CAN, EtherNet/IP, PROFINET, OPC-UA) and modern formats (JSON, XML, Protocol Buffers).

## Compatible Tools and Technologies

The platform is designed to support a wide range of modern tools and technologies:  
Node-RED, REST APIs, MQTT, WebSockets, GraphQL, gRPC, containers, time-series and relational databases, C++, Rust, Python, Node.js, Java, SQL, R, and more.

## Use Cases

- General-purpose industrial and home automation  
- Integration with machine vision systems  
- Educational and research use  
- DIY and maker projects with serious capabilities  
- As a real-time gateway between legacy machines and modern cloud-based services

## Current Status

The code is in an early stage, somewhere between alpha and beta. We are currently refactoring, but we already have a minimal functional base running on our BorrellPLC devices.

## License

This project is released under the **GNU Affero General Public License v3.0 (AGPL-3.0)**.

This license ensures that any modifications or derived works, even when used in a networked environment (e.g., as a backend service), must also be released as open source. It protects the core values of transparency, collaboration, and long-term freedom.

For details, see [LICENSE](./LICENSE).
