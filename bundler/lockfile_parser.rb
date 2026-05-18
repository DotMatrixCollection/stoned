require "bundler"

module Bundler
  class LockfileParser
    Spec = Struct.new(:name, :version, :source) do
      def full_name
        "#{name}-#{version}"
      end

      def to_s
        full_name
      end
    end

    Source = Struct.new(:type, :remote)

    attr_reader :specs, :platforms, :dependencies, :bundler_version, :sources

    def initialize(contents)
      @specs = []
      @platforms = []
      @dependencies = []
      @bundler_version = nil
      @sources = []
      parse(contents.to_s)
    end

    private

    def parse(contents)
      current_section = nil
      current_remote = nil

      contents.each_line do |line|
        stripped = line.chomp

        if stripped =~ /^(GEM|PATH|GIT|PLATFORMS|DEPENDENCIES|BUNDLED WITH)\s*$/
          current_section = $1
          current_remote = nil
          next
        end

        if (current_section == "GEM" || current_section == "PATH" || current_section == "GIT") &&
           stripped =~ /^\s{2}remote:\s+(.+)$/
          current_remote = $1
          @sources << Source.new(current_section.downcase, current_remote)
          next
        end

        if (current_section == "GEM" || current_section == "PATH" || current_section == "GIT") &&
           stripped =~ /^\s{4}(\S+)\s+\(([^)]+)\)/
          source = current_remote ? Source.new(current_section.downcase, current_remote) : nil
          @specs << Spec.new($1, $2, source)
          next
        end

        if current_section == "PLATFORMS" && stripped =~ /^\s{2}(\S+)/
          @platforms << $1
          next
        end

        if current_section == "DEPENDENCIES" && stripped =~ /^\s{2}(\S+)/
          @dependencies << $1.sub(/!$/, "")
          next
        end

        if current_section == "BUNDLED WITH" && stripped =~ /^\s+(\S+)/
          @bundler_version = $1
        end
      end
    end
  end
end
