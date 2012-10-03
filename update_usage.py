def make_usage_header(txt_filename):
  h_filename = txt_filename + '.h'
  out = open(h_filename, 'w')
  out.write('const char *usage =\n')
  for line in open(txt_filename):
    out.write('"'+line.rstrip('\n')+'\\n"\n')
  out.write(';\n');

make_usage_header("wavconv_usage.txt")
