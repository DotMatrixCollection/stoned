require "rubygems"
require "rbconfig"
require "bundler"

# Capture at load-time so __FILE__ inside methods works correctly.
BUNDLER_INLINE_STONED_ROOT = File.dirname(File.dirname(File.expand_path(__FILE__)))

# bundler/inline — gemfile() DSL for self-contained scripts.
# Usage (stoned form, uses block parameter):
#   require "bundler/inline"
#   gemfile do |g|
#     g.source "https://rubygems.org"
#     g.gem "some_gem"
#   end
#
# The no-param block form (real Bundler style) is not supported in this
# interpreter because instance_eval-based DSLs are not yet implemented.

class BundlerInlineDSL
  attr_reader :deps, :gem_source

  def initialize
    @deps       = []
    @gem_source = "https://rubygems.org"
  end

  def source(url, &block)
    @gem_source = url
  end

  def gem(name, *reqs, **opts)
    path = opts.delete(:path)
    opts.delete(:group); opts.delete(:groups)
    opts.delete(:require); opts.delete(:platforms); opts.delete(:platform)
    opts.delete(:github); opts.delete(:bitbucket); opts.delete(:git)
    opts.delete(:branch); opts.delete(:ref); opts.delete(:tag)
    @deps << { name: name.to_s, reqs: reqs.map(&:to_s), path: path }
  end

  def ruby(*); end
  def group(*, &block); yield self if block_given?; end
  def eval_gemfile(path); end
end

def gemfile(install: true, quiet: false, &block)
  dsl = BundlerInlineDSL.new
  yield dsl if block_given?

  tmp = "/tmp/bundler_inline_#{$$}_#{dsl.object_id}"
  system("mkdir", "-p", tmp)
  begin
    gemfile_path = File.join(tmp, "Gemfile")
    lines = ["source #{dsl.gem_source.inspect}"]
    dsl.deps.each do |dep|
      line = "gem #{dep[:name].inspect}"
      dep[:reqs].each { |r| line += ", #{r.inspect}" }
      line += ", path: #{dep[:path].inspect}" if dep[:path]
      lines << line
    end
    File.write(gemfile_path, lines.join("\n") + "\n")

    bundle_exe = File.join(BUNDLER_INLINE_STONED_ROOT, "exe", "bundle")
    ruby_cmd   = RbConfig.ruby
    old_env    = ENV["BUNDLE_GEMFILE"]
    ENV["BUNDLE_GEMFILE"] = gemfile_path
    Dir.chdir(tmp) do
      cmd = [ruby_cmd, bundle_exe, "install"]
      cmd << "--quiet" if quiet
      system(*cmd) if install
    end
    ENV["BUNDLE_GEMFILE"] = old_env

    # Set up load path
    Dir.chdir(tmp) do
      old_gemfile = ENV["BUNDLE_GEMFILE"]
      ENV["BUNDLE_GEMFILE"] = gemfile_path
      require File.join(BUNDLER_INLINE_STONED_ROOT, "bundler", "setup") rescue nil
      ENV["BUNDLE_GEMFILE"] = old_gemfile
    end
  ensure
    system("rm", "-rf", tmp) rescue nil
  end
end
