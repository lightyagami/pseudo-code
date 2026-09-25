import sys
import math
import random
import copy

# --- Pseudoc Runtime Helpers ---
_pc_files = {}
_pc_random_files = {}
def _pc_open_random(name):
    lines = []
    try:
        with open(name, 'r') as f:
            lines = [l.rstrip('\r\n') for l in f]
    except FileNotFoundError:
        pass
    _pc_random_files[name] = {'lines': lines, 'current': 1}
def _pc_seek(name, addr):
    if name not in _pc_random_files:
        raise RuntimeError(f"File '{name}' not open for RANDOM")
    _pc_random_files[name]['current'] = max(1, int(addr))
def _pc_put_record(name, val):
    if name not in _pc_random_files:
        raise RuntimeError(f"File '{name}' not open for RANDOM")
    rf = _pc_random_files[name]
    idx = rf['current'] - 1
    while len(rf['lines']) <= idx:
        rf['lines'].append('')
    rf['lines'][idx] = str(val)
    rf['current'] += 1
def _pc_get_record(name):
    if name not in _pc_random_files:
        raise RuntimeError(f"File '{name}' not open for RANDOM")
    rf = _pc_random_files[name]
    idx = rf['current'] - 1
    res = rf['lines'][idx] if idx < len(rf['lines']) else ''
    rf['current'] += 1
    return res
def _pc_close_random(name):
    if name in _pc_random_files:
        with open(name, 'w') as f:
            for l in _pc_random_files[name]['lines']:
                f.write(l + '\n')
        del _pc_random_files[name]
def _pc_open(name, mode):
    if mode == 'RANDOM':
        _pc_open_random(name)
        return
    m = 'r'
    if mode == 'WRITE': m = 'w'
    elif mode == 'APPEND': m = 'a'
    _pc_files[name] = open(name, m)
def _pc_read(name):
    return _pc_files[name].readline().rstrip('\r\n')
def _pc_write(name, val):
    _pc_files[name].write(str(val) + '\n')
def _pc_eof(name):
    if name in _pc_random_files:
        return _pc_random_files[name]['current'] > len(_pc_random_files[name]['lines'])
    f = _pc_files[name]
    pos = f.tell()
    line = f.readline()
    f.seek(pos)
    return len(line) == 0
def _pc_close(name):
    if name in _pc_random_files:
        _pc_close_random(name)
        return
    if name in _pc_files:
        _pc_files[name].close()
        del _pc_files[name]
def _pc_str(v):
    if v is True: return 'TRUE'
    if v is False: return 'FALSE'
    if isinstance(v, float): return f'{v:.10g}'
    return str(v)

class Student:
    def __init__(self):
        self.ID = 0
        self.Name = ''
        self.Marks = [0 for _ in range(6)]
        self.Average = 0.0
        self.Grade = ''
        self.Passed = False

def CalculateAverage(Marks):
    Marks = copy.deepcopy(Marks)
    Sum = 0
    i = 0
    Sum = 0
    for i in range(1, (5) + 1):
        Sum = (Sum + Marks[i])
    return (Sum / 5)

def CalculateGrade(Average):
    if (Average >= 80):
        return "A"
    else:
        if (Average >= 70):
            return "B"
        else:
            if (Average >= 60):
                return "C"
            else:
                if (Average >= 50):
                    return "D"
                else:
                    return "F"

def HasPassed(Marks):
    Marks = copy.deepcopy(Marks)
    i = 0
    for i in range(1, (5) + 1):
        if (Marks[i] < 40):
            return False
    return True

def CalculateStudentData(StudentData):
    StudentData.Average = CalculateAverage(StudentData.Marks)
    StudentData.Grade = CalculateGrade(StudentData.Average)
    StudentData.Passed = HasPassed(StudentData.Marks)

def SwapStudents(A, B):
    TempStudent = Student()
    TempStudent.__dict__.update(copy.deepcopy(A).__dict__)
    A.__dict__.update(copy.deepcopy(B).__dict__)
    B.__dict__.update(copy.deepcopy(TempStudent).__dict__)

def SortStudents(Data, Size):
    i = 0
    j = 0
    MaxIndex = 0
    for i in range(1, ((Size - 1)) + 1):
        MaxIndex = i
        for j in range((i + 1), (Size) + 1):
            if (Data[j].Average > Data[MaxIndex].Average):
                MaxIndex = j
            else:
                if (Data[j].Average == Data[MaxIndex].Average):
                    if (Data[j].ID < Data[MaxIndex].ID):
                        MaxIndex = j
        if (MaxIndex != i):
            SwapStudents(Data[i], Data[MaxIndex])

def BinarySearch(Data, Size, TargetID):
    Data = copy.deepcopy(Data)
    Low = 0
    High = 0
    Mid = 0
    Low = 1
    High = Size
    while (Low <= High):
        Mid = ((Low + High) // 2)
        if (Data[Mid].ID == TargetID):
            return Mid
        else:
            if (Data[Mid].ID < TargetID):
                Low = (Mid + 1)
            else:
                High = (Mid - 1)
    return (-(1))

def RecursiveFactorial(N):
    if (N <= 1):
        return 1
    else:
        return (N * RecursiveFactorial((N - 1)))

def IsPrime(N):
    i = 0
    if (N < 2):
        return False
    for i in range(2, ((N // 2)) + 1):
        if ((N % i) == 0):
            return False
    return True

def DisplayStudent(StudentData):
    StudentData = copy.deepcopy(StudentData)
    print(_pc_str("-----------------------------"), sep="")
    print(_pc_str("ID: "), _pc_str(StudentData.ID), sep="")
    print(_pc_str("Name: "), _pc_str(StudentData.Name), sep="")
    print(_pc_str("Marks:"), sep="")
    for k in range(1, (5) + 1):
        print(_pc_str("Subject "), _pc_str(k), _pc_str(": "), _pc_str(StudentData.Marks[k]), sep="")
    print(_pc_str("Average: "), _pc_str(StudentData.Average), sep="")
    print(_pc_str("Grade: "), _pc_str(StudentData.Grade), sep="")
    if (StudentData.Passed == True):
        print(_pc_str("Status: PASSED"), sep="")
    else:
        print(_pc_str("Status: FAILED"), sep="")

# --- Main Program ---
Students = [Student() for _ in range(11)]
Count = 0
i = 0
j = 0
k = 0
Choice = 0
SearchID = 0
Found = False
Total = 0
Highest = 0
Lowest = 0
Temp = 0
TempName = ''
TempAverage = 0.0
TempGrade = ''
TempPassed = False
AverageAll = 0.0
Count = 5
Students[1].ID = 104
Students[1].Name = "Alice"
Students[1].Marks[1] = 78
Students[1].Marks[2] = 85
Students[1].Marks[3] = 91
Students[1].Marks[4] = 67
Students[1].Marks[5] = 88
Students[2].ID = 101
Students[2].Name = "Bob"
Students[2].Marks[1] = 55
Students[2].Marks[2] = 61
Students[2].Marks[3] = 49
Students[2].Marks[4] = 73
Students[2].Marks[5] = 68
Students[3].ID = 109
Students[3].Name = "Charlie"
Students[3].Marks[1] = 92
Students[3].Marks[2] = 95
Students[3].Marks[3] = 89
Students[3].Marks[4] = 94
Students[3].Marks[5] = 90
Students[4].ID = 103
Students[4].Name = "Diana"
Students[4].Marks[1] = 35
Students[4].Marks[2] = 76
Students[4].Marks[3] = 82
Students[4].Marks[4] = 64
Students[4].Marks[5] = 71
Students[5].ID = 107
Students[5].Name = "Ethan"
Students[5].Marks[1] = 45
Students[5].Marks[2] = 51
Students[5].Marks[3] = 39
Students[5].Marks[4] = 48
Students[5].Marks[5] = 57
print(_pc_str("CALCULATING STUDENT DATA..."), sep="")
print(_pc_str(""), sep="")
for i in range(1, (Count) + 1):
    CalculateStudentData(Students[i])
print(_pc_str("ORIGINAL STUDENTS"), sep="")
for i in range(1, (Count) + 1):
    DisplayStudent(Students[i])
Highest = Students[1].Marks[1]
Lowest = Students[1].Marks[1]
Total = 0
for i in range(1, (Count) + 1):
    for j in range(1, (5) + 1):
        Total = (Total + Students[i].Marks[j])
        if (Students[i].Marks[j] > Highest):
            Highest = Students[i].Marks[j]
        if (Students[i].Marks[j] < Lowest):
            Lowest = Students[i].Marks[j]
AverageAll = (Total / (Count * 5))
print(_pc_str(""), sep="")
print(_pc_str("CLASS STATISTICS"), sep="")
print(_pc_str("Total Marks: "), _pc_str(Total), sep="")
print(_pc_str("Average Mark: "), _pc_str(AverageAll), sep="")
print(_pc_str("Highest Mark: "), _pc_str(Highest), sep="")
print(_pc_str("Lowest Mark: "), _pc_str(Lowest), sep="")
print(_pc_str(""), sep="")
print(_pc_str("SORTING STUDENTS..."), sep="")
SortStudents(Students, Count)
print(_pc_str(""), sep="")
print(_pc_str("STUDENTS SORTED BY AVERAGE"), sep="")
for i in range(1, (Count) + 1):
    print(_pc_str(i), _pc_str(". "), _pc_str(Students[i].Name), _pc_str(" - "), _pc_str(Students[i].Average), _pc_str(" - "), _pc_str(Students[i].Grade), sep="")
print(_pc_str(""), sep="")
print(_pc_str("SEARCH STUDENT BY ID"), sep="")
SearchID = 103
Found = False
for i in range(1, (Count) + 1):
    if (Students[i].ID == SearchID):
        Found = True
        print(_pc_str("Student found at position "), _pc_str(i), sep="")
        DisplayStudent(Students[i])
if (Found == False):
    print(_pc_str("Student not found"), sep="")
print(_pc_str(""), sep="")
print(_pc_str("PRIME NUMBER TEST"), sep="")
for i in range(1, (20) + 1):
    if IsPrime(i):
        print(_pc_str(i), _pc_str(" is prime"), sep="")
print(_pc_str(""), sep="")
print(_pc_str("RECURSIVE FACTORIAL TEST"), sep="")
for i in range(1, (10) + 1):
    print(_pc_str(i), _pc_str("! = "), _pc_str(RecursiveFactorial(i)), sep="")
print(_pc_str(""), sep="")
print(_pc_str("CASE TEST"), sep="")
Choice = 3
if Choice == 1:
    print(_pc_str("Option One"), sep="")
elif Choice == 2:
    print(_pc_str("Option Two"), sep="")
elif Choice == 3:
    print(_pc_str("Option Three"), sep="")
elif Choice == 4:
    print(_pc_str("Option Four"), sep="")
else:
    print(_pc_str("Invalid Option"), sep="")
