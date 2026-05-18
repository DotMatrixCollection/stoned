require "bundler"

module Bundler
  class RubyVersion
    attr_reader :versions, :engine, :engine_version, :patchlevel

    def initialize(versions, engine = nil, engine_version = nil, patchlevel = nil)
      @versions = Array(versions).map(&:to_s)
      @engine = engine
      @engine_version = engine_version
      @patchlevel = patchlevel
    end

    def self.system
      new(RUBY_VERSION, defined?(RUBY_ENGINE) ? RUBY_ENGINE : "ruby", RUBY_VERSION, defined?(RUBY_PATCHLEVEL) ? RUBY_PATCHLEVEL.to_s : nil)
    end

    def to_s
      @versions.join(", ")
    end

    def diff(other)
      return [] if other && other.versions == versions
      ["versions"]
    end
  end
end
