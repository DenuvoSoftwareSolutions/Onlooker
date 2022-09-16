import sys
import json

def get_output_name(infile: str):
    dotidx = infile.rfind('.')
    if dotidx < 0:
        dotidx = len(infile)
    base = infile[:dotidx]
    extension = infile[dotidx:]
    return f'{base}.log{extension}'

def main():
    if len(sys.argv) < 2:
        print("Usage: convert.py trace.json")
        sys.exit(1)

    infile = sys.argv[1]
    outfile = get_output_name(infile)
    outlog = {}
    outlines = []

    def appendlog(time, line):
        if time not in outlog:
            outlog[time] = []
        outlog[time].append(line)
        outlines.append(line)

    def prettycmd(cmd, args):
        for i in range(0, len(args)):
            arg: str = args[i]
            if " " in arg or "\n" in arg:
                arg = f"\"{arg}\"" 
            args[i] = arg
        return cmd + "(" + str.join(" ", args) + ")"

    with open(infile, "r") as f:
        curfile = None
        for line in f:
            entry = json.loads(line.rstrip())
            if "version" in entry:  # skip the first version line
                continue

            file = entry["file"]
            time = str(int(entry["time"] * 1000))

            if curfile != file:
                curfile = file
                appendlog(time, file + ":" + str(entry['line']))
            appendlog(time, "  " + prettycmd(entry["cmd"], entry["args"]))

    with open(outfile, "w") as f:
        json.dump(outlog, f, indent=2)
    with open(outfile + ".txt", "w") as f:
        f.write(str.join("\n", outlines))

if __name__ == "__main__":
    main()
