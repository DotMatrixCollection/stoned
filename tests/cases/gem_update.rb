$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_update_#{$$}"
server   = File.join(root, "server")
gem_home = File.join(root, "gemhome")
build_dir = File.join(root, "build")
[root, server, gem_home, build_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd = RbConfig.ruby.inspect
gem_exe  = File.expand_path("exe/gem", Dir.pwd)

# ── Build v1.0 ────────────────────────────────────────────────────────────────
def make_gem(build_dir, name, version, ruby_cmd, gem_exe)
  gdir = File.join(build_dir, "#{name}_#{version.gsub(".", "_")}")
  glib = File.join(gdir, "lib")
  [gdir, glib].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }
  File.write(File.join(gdir, "#{name}.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "#{name}"
      s.version = Gem::Version.new("#{version}")
      s.summary = "#{name}"
      s.files = ["lib/#{name}.rb", "#{name}.gemspec"]
      s.require_paths = ["lib"]
    end
  GEMSPEC
  File.write(File.join(glib, "#{name}.rb"), "module #{name.capitalize}; VERSION = '#{version}'; end\n")
  Dir.chdir(gdir) { `#{ruby_cmd} #{gem_exe.inspect} build #{name}.gemspec 2>&1` }
  Dir.glob(File.join(gdir, "#{name}-#{version}.gem")).first
end

v1 = make_gem(build_dir, "updgem", "1.0.0", ruby_cmd, gem_exe)
v2 = make_gem(build_dir, "updgem", "2.0.0", ruby_cmd, gem_exe)

# ── Set up fake server pointing at v2 as latest ───────────────────────────────
server_api  = File.join(server, "api", "v1", "gems")
server_gems = File.join(server, "gems")
[server_api, server_gems].each { |d| system("mkdir", "-p", d) }
File.write(File.join(server_api, "updgem.json"), '{"version": "2.0.0"}')
system("cp", v1, server_gems)
system("cp", v2, server_gems)

old_server = ENV["STONED_GEM_SERVER"]
ENV["STONED_GEM_SERVER"] = "file://#{server}"

# ── Install v1 directly from file ────────────────────────────────────────────
`#{ruby_cmd} #{gem_exe.inspect} install #{v1.inspect} 2>&1`

puts "--LIST-BEFORE--"
puts `#{ruby_cmd} #{gem_exe.inspect} list 2>&1`.chomp

# ── Update to v2 ─────────────────────────────────────────────────────────────
puts "--UPDATE--"
puts `#{ruby_cmd} #{gem_exe.inspect} update updgem 2>&1`.chomp

puts "--LIST-AFTER--"
puts `#{ruby_cmd} #{gem_exe.inspect} list 2>&1`.chomp

# ── Update when already at latest ────────────────────────────────────────────
puts "--ALREADY-LATEST--"
puts `#{ruby_cmd} #{gem_exe.inspect} update updgem 2>&1`.chomp

ENV["STONED_GEM_SERVER"] = old_server
ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
