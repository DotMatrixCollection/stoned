require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "lockfile_parser")

module Bundler
  class Definition
    def self.build(gemfile, lockfile, unlock)
      new(gemfile, lockfile, unlock)
    end

    def initialize(gemfile, lockfile, unlock)
      @gemfile = gemfile
      @lockfile = lockfile
      @unlock = unlock
      @dependencies = if File.exist?(gemfile.to_s)
        Bundler.send(:parse_gemfile_dependencies, gemfile.to_s)
      else
        []
      end
      @lockfile_parser = if File.exist?(lockfile.to_s)
        Bundler::LockfileParser.new(File.read(lockfile.to_s))
      else
        Bundler::LockfileParser.new("")
      end
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
      @lockfile_parser.specs
    end

    def platforms
      @lockfile_parser.platforms
    end

    def locked_gems
      @lockfile_parser
    end

    def lock(*_args)
      @lockfile_parser
    end
  end
end
