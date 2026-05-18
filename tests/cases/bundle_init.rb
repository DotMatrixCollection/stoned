$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_init_#{$$}"
app  = File.join(root, "app")
Dir.mkdir(root) unless Dir.exist?(root)
Dir.mkdir(app)  unless Dir.exist?(app)

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  # init creates a Gemfile in the current directory
  out = `#{ruby_cmd} #{bundle_exe.inspect} init 2>&1`.chomp
  puts out.sub(app, "/APP")

  puts "--EXISTS--"
  puts File.exist?("Gemfile")

  puts "--CONTENT--"
  puts File.read("Gemfile").include?("https://rubygems.org")

  # init again should fail
  out2, st2 = `#{ruby_cmd} #{bundle_exe.inspect} init 2>&1`.chomp, $?
  puts "--ALREADY-EXISTS--"
  puts out2.include?("already exists")
  puts st2.exitstatus
end

system("rm", "-rf", root)
