root = "/tmp/stoned_dir_entries_more"
Dir.mkdir(root) unless Dir.exist?(root)
File.write(File.join(root, "a.txt"), "a")
File.write(File.join(root, "b.rb"), "b")
p Dir.children(root).sort
p Dir.glob(File.join(root, "*.rb")).map { |p| File.basename(p) }
