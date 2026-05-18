$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundler_absolute_gemfile_#{$$}"
app      = File.join(root, "app")
work     = File.join(root, "work")
gem_home = File.join(root, "gemhome")
gem_dir  = File.join(root, "mygem")
lib_dir  = File.join(gem_dir, "lib")
[root, app, work, gem_home, gem_dir, lib_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(gem_dir, "mygem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "mygem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "mygem"
    s.files = ["lib/mygem.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(lib_dir, "mygem.rb"), "MYGEM_ABSOLUTE_OK = true\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "mygem", path: #{gem_dir.inspect}
GEMFILE

File.write(File.join(work, "use_absolute.rb"), <<~RUBY)
  ENV["BUNDLE_GEMFILE"] = #{File.join(app, "Gemfile").inspect}
  require "bundler"
  puts Bundler.root == #{app.inspect}
  puts Bundler.gemfile == #{File.join(app, "Gemfile").inspect}
  require "bundler/setup"
  require "mygem"
  puts MYGEM_ABSOLUTE_OK
RUBY

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
rubylib    = "RUBYLIB=#{Dir.pwd.inspect}"

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`
end

Dir.chdir(work) do
  out = `#{rubylib} #{ruby_cmd} #{File.join(work, "use_absolute.rb").inspect} 2>&1`.chomp
  puts "--ABSOLUTE-GEMFILE--"
  puts out
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
