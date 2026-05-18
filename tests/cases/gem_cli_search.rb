$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_gem_cli_search_#{$$}"
alpha = File.join(root, "alpha")
beta = File.join(root, "beta")
alpha_lib = File.join(alpha, "lib")
beta_lib = File.join(beta, "lib")
gem_home = File.join(root, "gemhome")
[root, alpha, beta, alpha_lib, beta_lib, gem_home].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(alpha_lib, "alpha.rb"), "module Alpha\nend\n")
File.write(File.join(beta_lib, "beta.rb"), "module Beta\nend\n")

File.write(File.join(alpha, "alpha.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "alpha"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "alpha gem"
    s.files = ["lib/alpha.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(beta, "beta.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "beta-tool"
    s.version = Gem::Version.new("2.1.0")
    s.summary = "beta gem"
    s.files = ["lib/beta.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

old_gem_home = ENV["GEM_HOME"]
old_home = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"] = root

gem_exe = File.expand_path("exe/gem", Dir.pwd)

[alpha, beta].each do |project|
  Dir.chdir(project) do
    spec = Dir.glob("*.gemspec").first
    puts `#{RbConfig.ruby} #{gem_exe} build #{spec}`.lines.first.chomp
    gem_file = Dir.glob("*.gem").first
    puts `#{RbConfig.ruby} #{gem_exe} install #{gem_file}`.chomp
  end
end

puts "--SEARCH-ALL--"
puts `#{RbConfig.ruby} #{gem_exe} search`.chomp
puts "--SEARCH-BETA--"
puts `#{RbConfig.ruby} #{gem_exe} search beta`.chomp
puts "--SEARCH-MISS--"
puts `#{RbConfig.ruby} #{gem_exe} search zzz`.chomp

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"] = old_home
system("rm", "-rf", root)
