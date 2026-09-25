// Pseudocode Compiler - Cambridge 9618 Web Playground
(function() {
  const PRESETS = {
    welcome: `// Welcome! Press Run (or Ctrl+Enter) to execute this code.

DECLARE name : STRING
DECLARE age  : INTEGER

OUTPUT "What is your name? "
INPUT name

OUTPUT "How old are you? "
INPUT age

OUTPUT "Hello, " & name & "!"

IF age < 18 THEN
    OUTPUT "You are a student."
ELSE
    OUTPUT "You are an adult."
ENDIF
`,

    linked_list: `// Cambridge 9618 Paper 4: Array-Based Linked List with Free List Pointer
TYPE ListNode
    DECLARE Data : INTEGER
    DECLARE NextPointer : INTEGER
ENDTYPE

DECLARE List : ARRAY[1:10] OF ListNode
DECLARE HeadPointer : INTEGER
DECLARE FreeListPointer : INTEGER
DECLARE i : INTEGER

PROCEDURE InitialiseList()
    HeadPointer <- -1
    FreeListPointer <- 1
    FOR i <- 1 TO 9
        List[i].NextPointer <- i + 1
        List[i].Data <- 0
    NEXT i
    List[10].NextPointer <- -1
    List[10].Data <- 0
ENDPROCEDURE

PROCEDURE InsertNode(NewItem : INTEGER)
    DECLARE NewNodeIndex : INTEGER
    DECLARE PreviousNode : INTEGER
    DECLARE CurrentNode : INTEGER

    IF FreeListPointer <> -1 THEN
        NewNodeIndex <- FreeListPointer
        FreeListPointer <- List[FreeListPointer].NextPointer
        List[NewNodeIndex].Data <- NewItem
        List[NewNodeIndex].NextPointer <- -1

        PreviousNode <- -1
        CurrentNode <- HeadPointer

        WHILE CurrentNode <> -1 AND List[CurrentNode].Data < NewItem
            PreviousNode <- CurrentNode
            CurrentNode <- List[CurrentNode].NextPointer
        ENDWHILE

        IF PreviousNode = -1 THEN
            List[NewNodeIndex].NextPointer <- HeadPointer
            HeadPointer <- NewNodeIndex
        ELSE
            List[NewNodeIndex].NextPointer <- CurrentNode
            List[PreviousNode].NextPointer <- NewNodeIndex
        ENDIF
    ELSE
        OUTPUT "Error: List is full"
    ENDIF
ENDPROCEDURE

PROCEDURE PrintList()
    DECLARE Current : INTEGER
    Current <- HeadPointer
    OUTPUT "--- Linked List Contents (Sorted) ---"
    WHILE Current <> -1
        OUTPUT "Node [Index ", Current, "]: Data = ", List[Current].Data, ", Next = ", List[Current].NextPointer
        Current <- List[Current].NextPointer
    ENDWHILE
ENDPROCEDURE

InitialiseList()
OUTPUT "Inserting items: 45, 12, 89, 34, 2"
InsertNode(45)
InsertNode(12)
InsertNode(89)
InsertNode(34)
InsertNode(2)
PrintList()
`,

    binary_tree: `// Cambridge 9618 Paper 4: Array-Based Binary Search Tree (BST)
TYPE TreeNode
    DECLARE LeftPointer  : INTEGER
    DECLARE Data         : INTEGER
    DECLARE RightPointer : INTEGER
ENDTYPE

DECLARE Tree : ARRAY[1:10] OF TreeNode
DECLARE RootPointer : INTEGER
DECLARE FreePointer : INTEGER
DECLARE i : INTEGER

PROCEDURE InitialiseTree()
    RootPointer <- -1
    FreePointer <- 1
    FOR i <- 1 TO 9
        Tree[i].LeftPointer <- i + 1
        Tree[i].Data <- 0
        Tree[i].RightPointer <- -1
    NEXT i
    Tree[10].LeftPointer <- -1
    Tree[10].Data <- 0
    Tree[10].RightPointer <- -1
ENDPROCEDURE

PROCEDURE InsertNode(NewItem : INTEGER)
    DECLARE NewNodeIndex : INTEGER
    DECLARE CurrentPointer : INTEGER
    DECLARE PreviousPointer : INTEGER
    DECLARE TurnedLeft : BOOLEAN

    IF FreePointer <> -1 THEN
        NewNodeIndex <- FreePointer
        FreePointer <- Tree[FreePointer].LeftPointer
        Tree[NewNodeIndex].LeftPointer <- -1
        Tree[NewNodeIndex].Data <- NewItem
        Tree[NewNodeIndex].RightPointer <- -1

        IF RootPointer = -1 THEN
            RootPointer <- NewNodeIndex
        ELSE
            CurrentPointer <- RootPointer
            TurnedLeft <- FALSE
            WHILE CurrentPointer <> -1
                PreviousPointer <- CurrentPointer
                IF NewItem < Tree[CurrentPointer].Data THEN
                    TurnedLeft <- TRUE
                    CurrentPointer <- Tree[CurrentPointer].LeftPointer
                ELSE
                    TurnedLeft <- FALSE
                    CurrentPointer <- Tree[CurrentPointer].RightPointer
                ENDIF
            ENDWHILE

            IF TurnedLeft = TRUE THEN
                Tree[PreviousPointer].LeftPointer <- NewNodeIndex
            ELSE
                Tree[PreviousPointer].RightPointer <- NewNodeIndex
            ENDIF
        ENDIF
    ELSE
        OUTPUT "Error: Tree is full"
    ENDIF
ENDPROCEDURE

PROCEDURE InOrderTraversal(Pointer : INTEGER)
    IF Pointer <> -1 THEN
        InOrderTraversal(Tree[Pointer].LeftPointer)
        OUTPUT "Value: ", Tree[Pointer].Data, " [at index ", Pointer, "]"
        InOrderTraversal(Tree[Pointer].RightPointer)
    ENDIF
ENDPROCEDURE

InitialiseTree()
OUTPUT "Inserting items: 50, 25, 75, 10, 30, 60, 85"
InsertNode(50)
InsertNode(25)
InsertNode(75)
InsertNode(10)
InsertNode(30)
InsertNode(60)
InsertNode(85)

OUTPUT ""
OUTPUT "--- In-Order Traversal (Sorted Output) ---"
InOrderTraversal(RootPointer)
`,

    queue_stack: `// Cambridge 9618 Paper 4: Stack and Linear Queue Implementation
DECLARE Stack : ARRAY[1:5] OF INTEGER
DECLARE TopOfStack : INTEGER
DECLARE MaxStack : INTEGER
MaxStack <- 5
TopOfStack <- 0

PROCEDURE Push(Item : INTEGER)
    IF TopOfStack = MaxStack THEN
        OUTPUT "Stack Overflow: Cannot push ", Item
    ELSE
        TopOfStack <- TopOfStack + 1
        Stack[TopOfStack] <- Item
        OUTPUT "Pushed: ", Item, " (Top at ", TopOfStack, ")"
    ENDIF
ENDPROCEDURE

FUNCTION Pop() RETURNS INTEGER
    DECLARE PoppedValue : INTEGER
    IF TopOfStack = 0 THEN
        OUTPUT "Stack Underflow: Stack is empty"
        RETURN -1
    ELSE
        PoppedValue <- Stack[TopOfStack]
        TopOfStack <- TopOfStack - 1
        RETURN PoppedValue
    ENDIF
ENDFUNCTION

DECLARE Queue : ARRAY[1:5] OF STRING
DECLARE HeadPointer : INTEGER
DECLARE TailPointer : INTEGER
DECLARE MaxQueue : INTEGER
MaxQueue <- 5
HeadPointer <- -1
TailPointer <- 0

PROCEDURE Enqueue(Item : STRING)
    IF TailPointer = MaxQueue THEN
        OUTPUT "Queue Full: Cannot enqueue '", Item, "'"
    ELSE
        TailPointer <- TailPointer + 1
        Queue[TailPointer] <- Item
        IF HeadPointer = -1 THEN
            HeadPointer <- 1
        ENDIF
        OUTPUT "Enqueued: '", Item, "' (Tail at ", TailPointer, ")"
    ENDIF
ENDPROCEDURE

FUNCTION Dequeue() RETURNS STRING
    DECLARE DequeuedItem : STRING
    IF HeadPointer = -1 OR HeadPointer > TailPointer THEN
        OUTPUT "Queue Empty: Cannot dequeue"
        RETURN ""
    ELSE
        DequeuedItem <- Queue[HeadPointer]
        HeadPointer <- HeadPointer + 1
        RETURN DequeuedItem
    ENDIF
ENDFUNCTION

OUTPUT "=== STACK DEMO ==="
Push(100)
Push(200)
OUTPUT "Popped from stack: ", Pop()
Push(300)
OUTPUT "Popped from stack: ", Pop()
OUTPUT "Popped from stack: ", Pop()

OUTPUT ""
OUTPUT "=== QUEUE DEMO ==="
Enqueue("First")
Enqueue("Second")
OUTPUT "Dequeued: ", Dequeue()
Enqueue("Third")
OUTPUT "Dequeued: ", Dequeue()
OUTPUT "Dequeued: ", Dequeue()
`,

    paper4_array: `// Cambridge 9618 Paper 4: Array-Returning Functions
FUNCTION GenerateSquares(limit : INTEGER) RETURNS ARRAY[1:5] OF INTEGER
    DECLARE arr : ARRAY[1:5] OF INTEGER
    DECLARE i : INTEGER
    FOR i <- 1 TO 5
        arr[i] <- i * i
    NEXT i
    RETURN arr
ENDFUNCTION

DECLARE original : ARRAY[1:5] OF INTEGER
DECLARE duplicate : ARRAY[1:5] OF INTEGER
DECLARE idx : INTEGER

// Whole-array assignment from function return
original <- GenerateSquares(5)

OUTPUT "--- Original Array Generated by Function ---"
FOR idx <- 1 TO 5
    OUTPUT "Square[", idx, "] = ", original[idx]
NEXT idx

// Whole-array deep copy assignment
duplicate <- original
duplicate[1] <- 999

OUTPUT ""
OUTPUT "--- After Mutating duplicate[1] <- 999 ---"
OUTPUT "original[1]  = ", original[1], " (remains intact)"
OUTPUT "duplicate[1] = ", duplicate[1], " (mutated independently)"
`,

    paper4_random_file: `// Cambridge 9618 Paper 4: Direct Access Random File I/O
TYPE StudentRecord
    DECLARE StudentID : INTEGER
    DECLARE Name : STRING
    DECLARE GPA : REAL
ENDTYPE

DECLARE s1 : StudentRecord
DECLARE s2 : StudentRecord
DECLARE readRec : StudentRecord
DECLARE filename : STRING

filename <- "students.dat"

s1.StudentID <- 101
s1.Name <- "Alice Smith"
s1.GPA <- 3.92

s2.StudentID <- 102
s2.Name <- "Bob Jones"
s2.GPA <- 3.65

OUTPUT "Writing direct access records to: ", filename
OPENFILE filename FOR RANDOM
SEEK filename, 2
PUTRECORD filename, s2
SEEK filename, 1
PUTRECORD filename, s1
CLOSEFILE filename

OUTPUT ""
OUTPUT "Reading records back via SEEK and GETRECORD:"
OPENFILE filename FOR RANDOM
SEEK filename, 1
GETRECORD filename, readRec
OUTPUT "Record 1 -> ID: ", readRec.StudentID, " | Name: ", readRec.Name, " | GPA: ", readRec.GPA

// Sequential advance to record 2
GETRECORD filename, readRec
OUTPUT "Record 2 -> ID: ", readRec.StudentID, " | Name: ", readRec.Name, " | GPA: ", readRec.GPA

OUTPUT "End of file reached? ", EOF(filename)
CLOSEFILE filename
`,

    oop_banking: `// Cambridge 9618: Object-Oriented Programming (Paper 4)
CLASS BankAccount
    PRIVATE AccountID : STRING
    PROTECTED Balance : REAL

    PUBLIC PROCEDURE NEW(id : STRING, initialBalance : REAL)
        AccountID <- id
        Balance <- initialBalance
    ENDPROCEDURE

    PUBLIC PROCEDURE Deposit(amount : REAL)
        Balance <- Balance + amount
    ENDPROCEDURE

    PUBLIC FUNCTION GetBalance() RETURNS REAL
        RETURN Balance
    ENDFUNCTION
ENDCLASS

CLASS SavingsAccount EXTENDS BankAccount
    PRIVATE InterestRate : REAL

    PUBLIC PROCEDURE NEW(id : STRING, initialBalance : REAL, rate : REAL)
        SUPER.NEW(id, initialBalance)
        InterestRate <- rate
    ENDPROCEDURE

    PUBLIC PROCEDURE AddInterest()
        Balance <- Balance + (Balance * InterestRate)
    ENDPROCEDURE
ENDCLASS

DECLARE acc : SavingsAccount
acc <- NEW SavingsAccount("SA-4091", 1000.0, 0.05)

OUTPUT "Initial Balance: $", acc.GetBalance()
acc.Deposit(500.0)
OUTPUT "After $500 Deposit: $", acc.GetBalance()
acc.AddInterest()
OUTPUT "After 5% Interest:  $", acc.GetBalance()
`,

    composite_params: `// Composite Types & Cambridge BYREF / BYVAL Semantics
TYPE Point2D
    DECLARE X : INTEGER
    DECLARE Y : INTEGER
ENDTYPE

PROCEDURE MoveByVal(p : Point2D, dx : INTEGER, dy : INTEGER)
    p.X <- p.X + dx
    p.Y <- p.Y + dy
ENDPROCEDURE

PROCEDURE MoveByRef(BYREF p : Point2D, dx : INTEGER, dy : INTEGER)
    p.X <- p.X + dx
    p.Y <- p.Y + dy
ENDPROCEDURE

DECLARE pt : Point2D
pt.X <- 10
pt.Y <- 20

OUTPUT "Starting Point: (", pt.X, ", ", pt.Y, ")"

// BYVAL (default in Cambridge) copies the record
CALL MoveByVal(pt, 5, 5)
OUTPUT "After BYVAL call: (", pt.X, ", ", pt.Y, ") [Unchanged]"

// BYREF explicitly mutates caller's instance
CALL MoveByRef(pt, 5, 5)
OUTPUT "After BYREF call: (", pt.X, ", ", pt.Y, ") [Updated]"
`,

    bubble_sort: `// Cambridge 9618 Algorithm: Bubble Sort
DECLARE list : ARRAY[1:6] OF INTEGER
DECLARE i : INTEGER
DECLARE j : INTEGER
DECLARE temp : INTEGER
DECLARE swapped : BOOLEAN

list[1] <- 45
list[2] <- 12
list[3] <- 89
list[4] <- 34
list[5] <- 7
list[6] <- 23

OUTPUT "Unsorted Array:"
FOR i <- 1 TO 6
    OUTPUT list[i]
NEXT i

REPEAT
    swapped <- FALSE
    FOR j <- 1 TO 5
        IF list[j] > list[j + 1] THEN
            temp <- list[j]
            list[j] <- list[j + 1]
            list[j + 1] <- temp
            swapped <- TRUE
        ENDIF
    NEXT j
UNTIL NOT swapped

OUTPUT ""
OUTPUT "Sorted Array:"
FOR i <- 1 TO 6
    OUTPUT list[i]
NEXT i
`,

    basics_tour: `// Cambridge 9618: Core Syntax Tour
DECLARE count : INTEGER
DECLARE total : REAL
DECLARE greeting : STRING
DECLARE flag : BOOLEAN

count <- 5
total <- 99.5
greeting <- "Welcome to Cambridge Pseudocode"
flag <- TRUE

OUTPUT greeting
OUTPUT "Count = ", count, " | Total = ", total

IF count > 0 AND flag THEN
    OUTPUT "Status: Active and verified."
ENDIF

OUTPUT "Counting down:"
WHILE count > 0
    OUTPUT count
    count <- count - 1
ENDWHILE
OUTPUT "Liftoff!"
`
  };

  const DEFAULT_STDIN = {
    welcome: "Alex\n20",
    paper4_array: "",
    paper4_random_file: "",
    oop_banking: "",
    composite_params: "",
    bubble_sort: "",
    basics_tour: ""
  };

  let wasmModule = null;
  let currentTab = 'vm';
  const tabCache = { vm: '', py: '', c: '', bc: '', check: '' };

  const codeEditor = document.getElementById('code-editor');
  const lineNumbers = document.getElementById('line-numbers');
  const stdinInput = document.getElementById('stdin-input');
  const outputContent = document.getElementById('output-content');
  const emptyState = document.getElementById('empty-state');
  const statusDot = document.getElementById('status-indicator');
  const statusText = document.getElementById('status-text');
  const lineCountDisplay = document.getElementById('line-count-display');
  const presetSelect = document.getElementById('preset-select');
  const btnRun = document.getElementById('btn-run');
  const btnCheck = document.getElementById('btn-check');
  const btnClear = document.getElementById('btn-clear');
  const btnCopyCode = document.getElementById('btn-copy-code');
  const btnCopyOutput = document.getElementById('btn-copy-output');
  const tabBtns = document.querySelectorAll('.tab-btn');

  function updateLineNumbers() {
    const lines = codeEditor.value.split('\n');
    const count = lines.length;
    let nums = '';
    for (let i = 1; i <= count; ++i) {
      nums += i + '\n';
    }
    lineNumbers.textContent = nums;
    lineCountDisplay.textContent = `${count} ${count === 1 ? 'line' : 'lines'}`;
  }

  codeEditor.addEventListener('scroll', () => {
    lineNumbers.scrollTop = codeEditor.scrollTop;
  });

  codeEditor.addEventListener('input', () => {
    updateLineNumbers();
  });

  function setStatus(text, state) {
    statusText.textContent = text;
    statusDot.className = `status-dot ${state}`;
  }

  function displayOutput(text) {
    if (!text && currentTab === 'vm') {
      emptyState.style.display = 'flex';
      outputContent.classList.remove('visible');
      outputContent.textContent = '';
    } else {
      emptyState.style.display = 'none';
      outputContent.classList.add('visible');
      outputContent.textContent = text || '(No output)';
    }
  }

  function clearOutputs() {
    tabCache.vm = '';
    tabCache.py = '';
    tabCache.c = '';
    tabCache.bc = '';
    tabCache.check = '';
    displayOutput('');
  }

  function runCurrentTab() {
    if (!wasmModule) {
      displayOutput("WebAssembly engine is initializing...");
      return;
    }
    const source = codeEditor.value;
    const input = stdinInput.value;
    setStatus('Running...', 'running');
    const t0 = performance.now();

    try {
      if (currentTab === 'vm') {
        const out = wasmModule.wasm_run_vm(source, input);
        tabCache.vm = out;
        displayOutput(out);
      } else if (currentTab === 'py') {
        const out = wasmModule.wasm_emit_py(source);
        tabCache.py = out;
        displayOutput(out);
      } else if (currentTab === 'c') {
        const out = wasmModule.wasm_emit_c(source);
        tabCache.c = out;
        displayOutput(out);
      } else if (currentTab === 'bc') {
        const out = wasmModule.wasm_dump_bytecode(source);
        tabCache.bc = out;
        displayOutput(out);
      } else if (currentTab === 'check') {
        const out = wasmModule.wasm_check(source);
        tabCache.check = out;
        displayOutput(out);
      }
      const dt = (performance.now() - t0).toFixed(1);
      setStatus(`Ready (${dt} ms)`, 'ready');
    } catch (err) {
      displayOutput("Error: " + err.message);
      setStatus('Error', 'error');
    }
  }

  function loadPreset(key) {
    if (PRESETS[key]) {
      codeEditor.value = PRESETS[key];
      stdinInput.value = DEFAULT_STDIN[key] || "";
      updateLineNumbers();
      clearOutputs();
      runCurrentTab();
    }
  }

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      tabBtns.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      currentTab = btn.getAttribute('data-tab');

      if (tabCache[currentTab]) {
        displayOutput(tabCache[currentTab]);
      } else {
        runCurrentTab();
      }
    });
  });

  btnRun.addEventListener('click', () => {
    currentTab = 'vm';
    tabBtns.forEach(b => b.classList.toggle('active', b.getAttribute('data-tab') === 'vm'));
    runCurrentTab();
  });

  btnCheck.addEventListener('click', () => {
    currentTab = 'check';
    tabBtns.forEach(b => b.classList.toggle('active', b.getAttribute('data-tab') === 'check'));
    runCurrentTab();
  });

  presetSelect.addEventListener('change', (e) => {
    loadPreset(e.target.value);
  });

  btnClear.addEventListener('click', () => {
    codeEditor.value = '';
    updateLineNumbers();
    clearOutputs();
    setStatus('Ready', 'ready');
  });

  btnCopyCode.addEventListener('click', () => {
    navigator.clipboard.writeText(codeEditor.value).then(() => {
      const prev = btnCopyCode.textContent;
      btnCopyCode.textContent = "Copied";
      setTimeout(() => btnCopyCode.textContent = prev, 1200);
    });
  });

  btnCopyOutput.addEventListener('click', () => {
    navigator.clipboard.writeText(outputContent.textContent).then(() => {
      const prev = btnCopyOutput.textContent;
      btnCopyOutput.textContent = "Copied";
      setTimeout(() => btnCopyOutput.textContent = prev, 1200);
    });
  });

  const btnShare = document.getElementById('btn-share');
  const shareLabel = document.getElementById('share-label');
  const btnExportPseudo = document.getElementById('btn-export-pseudo');
  const btnExportPy = document.getElementById('btn-export-py');
  const btnExportC = document.getElementById('btn-export-c');
  const btnFormat = document.getElementById('btn-format');

  function downloadFile(filename, text) {
    const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function encodeCodeForUrl(code) {
    try {
      return encodeURIComponent(btoa(unescape(encodeURIComponent(code))));
    } catch (e) {
      return encodeURIComponent(code);
    }
  }

  function decodeCodeFromUrl(hash) {
    try {
      return decodeURIComponent(escape(atob(decodeURIComponent(hash))));
    } catch (e) {
      return decodeURIComponent(hash);
    }
  }

  btnShare.addEventListener('click', () => {
    const code = codeEditor.value;
    const encoded = encodeCodeForUrl(code);
    const url = window.location.origin + window.location.pathname + '#code=' + encoded;
    window.history.replaceState(null, '', url);
    navigator.clipboard.writeText(url).then(() => {
      const prev = shareLabel ? shareLabel.textContent : "Share";
      if (shareLabel) shareLabel.textContent = "Copied!";
      setTimeout(() => { if (shareLabel) shareLabel.textContent = prev; }, 1800);
    });
  });

  btnExportPseudo.addEventListener('click', () => {
    downloadFile('main.pseudo', codeEditor.value);
  });

  btnExportPy.addEventListener('click', () => {
    if (!wasmModule) return;
    const py = wasmModule.wasm_emit_py(codeEditor.value);
    downloadFile('main.py', py);
  });

  btnExportC.addEventListener('click', () => {
    if (!wasmModule) return;
    const c = wasmModule.wasm_emit_c(codeEditor.value);
    downloadFile('main.c', c);
  });

  if (btnFormat) {
    btnFormat.addEventListener('click', () => {
      if (!wasmModule || !wasmModule.wasm_format) return;
      const formatted = wasmModule.wasm_format(codeEditor.value);
      codeEditor.value = formatted;
      updateLineNumbers();
    });
  }

  // Editor shortcuts: Tab and Ctrl+Enter / Cmd+Enter
  codeEditor.addEventListener('keydown', (e) => {
    if (e.key === 'Tab') {
      e.preventDefault();
      const start = codeEditor.selectionStart;
      const end = codeEditor.selectionEnd;
      codeEditor.value = codeEditor.value.substring(0, start) + "    " + codeEditor.value.substring(end);
      codeEditor.selectionStart = codeEditor.selectionEnd = start + 4;
      updateLineNumbers();
    } else if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      btnRun.click();
    }
  });

  // Initialize WebAssembly
  if (typeof createPseudocModule === 'function') {
    createPseudocModule().then(Module => {
      wasmModule = {
        wasm_run_vm: Module.cwrap('wasm_run_vm', 'string', ['string', 'string']),
        wasm_emit_c: Module.cwrap('wasm_emit_c', 'string', ['string']),
        wasm_emit_py: Module.cwrap('wasm_emit_py', 'string', ['string']),
        wasm_dump_bytecode: Module.cwrap('wasm_dump_bytecode', 'string', ['string']),
        wasm_check: Module.cwrap('wasm_check', 'string', ['string']),
        wasm_c_to_pseudo: Module.cwrap('wasm_c_to_pseudo', 'string', ['string']),
        wasm_format: Module.cwrap('wasm_format', 'string', ['string']),
      };
      setStatus('Ready', 'ready');

      // Check if URL has #code=
      const hash = window.location.hash;
      if (hash && hash.startsWith('#code=')) {
        const decoded = decodeCodeFromUrl(hash.substring(6));
        codeEditor.value = decoded;
        updateLineNumbers();
        clearOutputs();
        runCurrentTab();
      } else {
        loadPreset('welcome');
      }
    }).catch(err => {
      displayOutput("Failed to load WebAssembly module: " + err);
      setStatus('Error', 'error');
    });
  } else {
    displayOutput("WebAssembly script not found.");
    setStatus('Error', 'error');
  }
})();
