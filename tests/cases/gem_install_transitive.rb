$stdout.sync = true

require "rbconfig"

# Validates that gem install follows runtime_dependencies transitively:
# installing gema (→ gemb → gemc) installs all three.

root = "/tmp/stoned_gem_install_transitive_#{$$}"
server    = File.join(root, "server")
gem_home  = File.join(root, "gemhome")
build_dir = File.join(root, "build")
[root, server, gem_home, build_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

def make_gem(build_dir, name, version, deps, ruby_cmd, gem_exe)
  gdir = File.join(build_dir, name)
  glib = File.join(gdir, "lib")
  [gdir, glib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
  dep_lines = deps.map { |d| "  s.add_runtime_dependency \"#{d}\"" }.join("\n")
  File.write(File.join(gdir, "#{name}.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "#{name}"
      s.version = Gem::Version.new("#{version}")
      s.summary = "#{name}"
      s.files = ["lib/#{name}.rb", "#{name}.gemspec"]
      s.require_paths = ["lib"]
    #{dep_lines}
    end
  GEMSPEC
  File.write(File.join(glib, "#{name}.rb"), "module #{name.capitalize}; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

ga = make_gem(build_dir, "tgema", "1.0.0", ["tgemb"], ruby_cmd, gem_exe)
gb = make_gem(build_dir, "tgemb", "1.0.0", ["tgemc"], ruby_cmd, gem_exe)
gc = make_gem(build_dir, "tgemc", "1.0.0", [],        ruby_cmd, gem_exe)

server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
[["tgema", "1.0.0", ga], ["tgemb", "1.0.0", gb], ["tgemc", "1.0.0", gc]].each do |name, ver, path|
  File.write(File.join(server_api, "#{name}.json"), "{\"version\": \"#{ver}\"}")
  system("cp", path, server_gems)
end

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

# gem install tgema should also install tgemb and tgemc
out = `#{ruby_cmd} #{gem_exe.inspect} install tgema 2>&1`.chomp
puts out

puts "--LIST--"
puts `#{ruby_cmd} #{gem_exe.inspect} list 2>&1`.chomp

puts "--TGEMA-INSTALLED--"
puts File.directory?(File.join(gem_home, "gems", "tgema-1.0.0"))
puts "--TGEMB-INSTALLED--"
puts File.directory?(File.join(gem_home, "gems", "tgemb-1.0.0"))
puts "--TGEMC-INSTALLED--"
puts File.directory?(File.join(gem_home, "gems", "tgemc-1.0.0"))

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
