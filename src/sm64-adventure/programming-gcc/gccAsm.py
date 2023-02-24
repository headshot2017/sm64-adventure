def gccAsm(file):
    lines = open(file).read().split("\n")
    inAsm = False

    for i in range(len(lines)):
        tabs = lines[i].count("\t")
        line = lines[i].replace("\t", "")
        if line.startswith("__asm"):
            inAsm = True
            lines[i] = "asm"
            for j in range(tabs):
                lines[i] = "\t" + lines[i]
        elif line == "{" and inAsm:
            lines[i] = "("
            for j in range(tabs):
                lines[i] = "\t" + lines[i]
        elif line == "}" and inAsm:
            inAsm = False
            lines[i] = ");"
            for j in range(tabs):
                lines[i] = "\t" + lines[i]
        elif inAsm: # an asm line
            lines[i] = '"' + line + '\\n"'
            for j in range(tabs):
                lines[i] = "\t" + lines[i]

    return "\n".join(lines)

f = raw_input("Enter header file\n> ")
result = gccAsm(f)
open(f, "w").write(result)
