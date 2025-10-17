## PyRBD3

An open-source Python tool for Reliability Block Diagram (RBD) evaluation, which is suitable for complex systems and very large networks with bidirectional links between components and cyclic loops. PyRBD3 addresses the efficiency limitations of [PyRBD++](https://github.com/shakthij98/PyRBD_plusplus) by combining Boolean techniques with the Minimal Path Set (MCS) method. The Boolean algorithm is implemented in C++ and integrated with Python, significantly improving computational performance.

If you use this tool, please cite us as follows. S. Janardhanan, Y. Chen, W. Kellerer, C. Mas-Machuca, "Leveraging Minimal Path Sets for Reliability Block Diagram Evaluation", submitted to IEEE Future Networks World Forum 2025. 

## Recommended Environment

- WSL2 with Ubuntu 22.04
- Python 3.10 or higher
- GCC 11/G++ 11

## Python Environment

```bash
pip install pyrbd3
```

## Run Demo

```bash
python demo.py
``` 

## Topology Reference
**Germany_17**: [SNDlib 1.0-survivable network design library](https://sndlib.put.poznan.pl/home.action)
