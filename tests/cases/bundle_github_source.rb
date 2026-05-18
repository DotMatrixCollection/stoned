$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundle_github_source_#{$$}"
app      = File.join(root, "app")
gem_home = File.join(root, "gemhome")
[root, app, gem_home].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

# Gemfile with github: shorthand — treated as git source (warning, no install)
File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "some_lib", github: "example/some_lib"
  gem "other_lib", bitbucket: "example/other_lib", branch: "main"
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  # github/bitbucket gems are treated as git sources and skipped with a warning
  out = `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
  puts "--GIT-WARN--"
  puts out.include?("git sources not yet supported") || out.include?("WARNING")
  # Exit status should be 0 (warnings don't fail the install)
  puts $?.exitstatus
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
