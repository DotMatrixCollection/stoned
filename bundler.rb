require "rubygems"

module Bundler
  VERSION = "2.5.0.stoned"

  class GemfileNotFound < RuntimeError; end
  class GemNotFound    < RuntimeError; end
  class BundlerError   < RuntimeError; end
  class LockfileError  < BundlerError; end

  class << self
    def root
      gemfile = ENV["BUNDLE_GEMFILE"] || "Gemfile"
      dir = Dir.pwd
      loop do
        return Pathname.new(dir) if File.exist?(File.join(dir, gemfile))
        parent = File.dirname(dir)
        break if parent == dir
        dir = parent
      end
      raise GemfileNotFound, "Could not locate Gemfile"
    end

    def gemfile
      ENV["BUNDLE_GEMFILE"] || "Gemfile"
    end

    def require(*groups)
      # Minimal stub: groups activation handled by bundler/setup
    end

    def setup(*groups)
      require "bundler/setup"
      self
    end

    def environment
      self
    end

    def with_unbundled_env(&block)
      block.call if block_given?
    end
    alias unbundled_env with_unbundled_env
    alias with_clean_env with_unbundled_env
    alias clean_env with_unbundled_env
  end
end
