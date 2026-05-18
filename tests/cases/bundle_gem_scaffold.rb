$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_gem_scaffold_#{$$}"
Dir.mkdir(root) unless Dir.exist?(root)

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(root) do
  out = `#{ruby_cmd} #{bundle_exe.inspect} gem mygem 2>&1`.chomp
  puts out

  puts "--FILES--"
  puts File.exist?("mygem/mygem.gemspec")
  puts File.exist?("mygem/lib/mygem.rb")
  puts File.exist?("mygem/lib/mygem/version.rb")
  puts File.exist?("mygem/Gemfile")
  puts File.exist?("mygem/README.md")

  puts "--VERSION--"
  content = File.read("mygem/lib/mygem/version.rb")
  puts content.include?("VERSION")
  puts content.include?("Mygem")

  puts "--GEMSPEC--"
  spec_src = File.read("mygem/mygem.gemspec")
  puts spec_src.include?("mygem")

  # gem again should fail
  out2 = `#{ruby_cmd} #{bundle_exe.inspect} gem mygem 2>&1`.chomp
  puts "--ALREADY-EXISTS--"
  puts out2.include?("already exists")
end

system("rm", "-rf", root)
