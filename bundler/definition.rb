require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "dsl")
require File.join(BUNDLER_RB_DIR, "bundler", "lockfile_parser")
require File.join(BUNDLER_RB_DIR, "bundler", "spec_set")

module Bundler
  class Definition
    attr_reader :sources

    def self.build(gemfile, lockfile, unlock)
      new(gemfile, lockfile, unlock)
    end

    def initialize(gemfile, lockfile, unlock)
      @gemfile = gemfile
      @lockfile = lockfile
      @unlock = unlock
      dsl = if File.exist?(gemfile.to_s)
        Bundler::DSL.evaluate(gemfile.to_s)
      else
        Bundler::DSL.new
      end
      @dependencies = dsl.dependencies
      @sources = dsl.sources
      @lockfile_parser = if File.exist?(lockfile.to_s)
        Bundler::LockfileParser.new(File.read(lockfile.to_s))
      else
        Bundler::LockfileParser.new("")
      end
      @specs = Bundler::SpecSet.new(@lockfile_parser.specs)
    end

    def gemfile
      @gemfile
    end

    def lockfile
      @lockfile
    end

    def dependencies
      @dependencies
    end

    def specs
      @specs
    end

    def requested_specs
      @requested_specs ||= @specs.for(@dependencies)
    end

    def platforms
      @lockfile_parser.platforms
    end

    def source_requirements
      @sources
    end

    def locked_gems
      @lockfile_parser
    end

    def locked_specs
      @specs
    end

    def lock(*_args)
      @lockfile_parser
    end

    def resolve
      requested_specs
    end

    def resolve_remotely!
      requested_specs
    end

    def missing_specs?
      false
    end

    def nothing_changed?
      true
    end
  end
end
