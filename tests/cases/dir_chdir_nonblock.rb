root = "/tmp/stoned_dir_mkdir_chdir"
Dir.mkdir(root) unless Dir.exist?(root)
Dir.chdir(root)
p File.basename(Dir.pwd)
Dir.chdir("/")
p Dir.pwd
