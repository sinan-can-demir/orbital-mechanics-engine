# Developer Notes

## 04/02/2026

I want to use another data format because csv has become a problem since its I/O and storage become harder as our # of steps increases. Therefore, I looked up online and I am in between 2 options:  

- Option 1 **HDF5**  
    - This is data file format created by NASA explicity for scientific data.  
    - It is a structured data format, has a unique structure.  
    - inside the file you can create another files with hierchy and load these parts by themselves
    - it has a complex setup  

- Option 2: **Binary**  
    - This option is simpler but it is not human readable.  
    - Very fast and takes less space  
    - easier setup than HDF5  

**My solution:**  
I will follow a hybrid approach and try to implement the following structure.  
```bash
simulation → HDF5 (source of truth)
           → optional .bin cache (viewer)
```
In this way i will have both small and fast, and human readable data.  

I think I will add a utility script for run file picking. It will orchastrate automation in `Makefile` too.  

---

