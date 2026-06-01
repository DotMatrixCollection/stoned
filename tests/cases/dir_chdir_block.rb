root = "/tmp/stoned_dir_chdir_block"
Dir.mkdir(root) unless Dir.exist?(root)
before = Dir.pwd
value = Dir.chdir(root) { [Dir.pwd, File.basename(Dir.pwd)] }
p before == Dir.pwd
p value[1]
