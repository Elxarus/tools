import glob

def make_usage_header(txt_filename):
  h_filename = txt_filename + '.h'
  out = open(h_filename, 'w')
  out.write('const char *usage =\n')
  for line in open(txt_filename):
    out.write('"'+line.rstrip('\n')+'\\n"\n')
  out.write(';\n');

if __name__ == '__main__':
  for f in glob.glob('*_usage.txt'):
    print "Updating %s" % f
    make_usage_header(f)
