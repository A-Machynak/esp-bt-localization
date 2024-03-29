import os, subprocess, gzip, argparse

dir_path = os.path.dirname(os.path.realpath(__file__)).replace("\\", "/")

parser = argparse.ArgumentParser(prog="zip", description="Compresses an html and js file and embeds the js file into the html file")
parser.add_argument("html_min_file", nargs="?", default=dir_path + "/visualize.min.html")
parser.add_argument("js_min_file", nargs="?", default=dir_path + "/visualize.min.js")
parser.add_argument("html_file", help="Used for --automatic", nargs="?", default=dir_path + "/visualize.html")
parser.add_argument("js_file", help="Used for --automatic", nargs="?", default=dir_path + "/visualize.js")
parser.add_argument("out_file", nargs="?", default="compressed.h")
parser.add_argument("-a", "--automatic",
					help="Executes html-minifier and google-closure-compiler to minify html_file and js_file",
					action=argparse.BooleanOptionalAction,
					default=False)
args = parser.parse_args()

# Read
content_html = ""
content_js = ""
if args.automatic:
	# Run html-minifier and google-closure-compiler
	print(f"Minifying html file: \"{args.html_file}\", Js file: \"{args.js_file}\"")

	# HTML
	html_minif = subprocess.run([
		"html-minifier",
		"--collapse-whitespace",
		"--remove-comments",
		"--remove-optional-tags",
		"--remove-redundant-attributes",
		"--remove-script-type-attributes",
		"--remove-tag-whitespace",
		args.html_file], shell=True, capture_output=True)
	content_html = html_minif.stdout.decode('ascii')
	if html_minif.returncode != 0:
		print(f"HTML minification error: {html_minif.stderr.decode()}")
		exit(1)

	# JS
	cmd = "google-closure-compiler -O ADVANCED --js " + args.js_file
	js_minif = subprocess.run(cmd, shell=True, capture_output=True)
	if js_minif.returncode != 0:
		print(f"JS minification error: {js_minif.stderr.decode()}")
		exit(1)
	content_js = js_minif.stdout.decode('ascii')
else:
	print(f"Html file: \"{args.html_min_file}\", Js file: \"{args.js_min_file}\"")
	with open(args.html_min_file, "r") as f:
		content_html = f.read()
	with open(args.js_min_file, "r") as f:
		content_js = f.read()

# Embed JS
idx = content_html.rfind("</body>")
if idx == -1:
	print("Couldn't find </body> tag to embed JS. Inserting at the end")
	idx = len(content_html)
content = (content_html[:idx] + "<script>" + content_js + "</script>" + content_html[idx:])

# Compress
compressed = gzip.compress(content.encode("ascii"))
with open(args.out_file, "w") as f:
	f.write("#pragma once\n\n")
	f.write("#include <array>\n\n")
	f.write("/// Compressed index page.\n")
	f.write("static const std::array IndexPageGzip = std::to_array<char>({ ")
	for byte in compressed:
		f.write(f"0x{byte:02x}, ")
	f.write("});\n")
	print(f"Saved into \"{args.out_file}\"")
	print(f"Original size: {len(content)}; Compressed size: {len(compressed)}")
